#include "VMesh/model.hpp"
#include "VMesh/voxelGrid.hpp"

#include "progressBar.hpp"
#include "octree.hpp"

#include <array>
#include <vector>
#include <iostream>
#include <fstream>

#include <boost/program_options.hpp>
#include <thread_pool/thread_pool.h>

namespace po = boost::program_options;

int main(int argc, char** argv) {
  uint resolution, subdivisionlevel;
  bool isSvdag, isVerbose, isCompressed, isConvert, isTribox, isBinary, isConvertVox;
  float addColourThreshold;
  std::string in, out, paletteOut, scaleMode, inputType, addColourThresholdStr;

  // VMesh::Palette testPalette;
  // for (uint r = 0; r <= 255; ++r) {
  //   for (uint g = 0; g <= 255; ++g) for (uint b = 0; b <= 255; ++b)
  //     testPalette.addColour({r / 255.f, g / 255.f, b / 255.f}, 0.01f);
  //   std::println("r: {} / 255, palette size: {}", r, testPalette.size());
  // }
  // std::println("testPaletteSize: {}", testPalette.size());
  // return 0;
  
  // ##############
  // - Parse args -
  // ##############

  po::options_description visibleOptions("Options", 100, 40);
  visibleOptions.add_options()
    ("help,h", "produce help message")
    ("verbose,v", po::bool_switch(&isVerbose), "verbose output")
    ("compressed,C", po::bool_switch(&isCompressed), "compress voxel data")
    ("svdag,S", po::bool_switch(&isSvdag), "generate a Sparse Voxel Octree instead of a normal voxel grid")
    ("resolution,R", po::value<uint>(&resolution)->default_value(128), "set voxel grid resolution")
    ("subdivision-level,L", po::value<uint>(&subdivisionlevel)->default_value(0), "set depth to generate initial subtrees before combining")
    ("scale-mode", po::value<std::string>(&scaleMode)->default_value("proportional"), "scaling mode either (proportional, stretch, none)")
    ("voxel-to-svdag", po::bool_switch(&isConvert), "input voxel binary file and output svdag")
    ("magicavoxel-to-svdag", po::bool_switch(&isConvertVox), "input magicavoxel .vox file and output svdag")
    ("tribox", po::bool_switch(&isTribox), "use triangle box intersections instead of DDA voxelization, it tends to be faster on low resolutions(<512) however it only generates binary data")
    ("binary,B", po::bool_switch(&isBinary), "generate binary voxel data instead of coloured voxel data")
    ("colourThreshold", po::value<std::string>(&addColourThresholdStr)->default_value("0.01"), "set the normalized percentage colour difference threshold that is required for a new colour to be added to the palette")
  ;

  po::options_description hiddenOptions("Hidden");
  hiddenOptions.add_options()
    ("in", po::value<std::string>(&in), "input file path")
  ;

  po::positional_options_description p;
  p.add("in", 1);

  po::options_description options("Options", 100, 40);
  options.add(visibleOptions).add(hiddenOptions);

  po::variables_map vm;
  try {
    po::store(po::command_line_parser(argc, argv).options(options).positional(p).run(), vm);
  }
  catch (boost::wrapexcept<po::invalid_option_value>& pErr) {
    std::println("{}, use -h for help", pErr.what());
    return 1;
  }
  catch (boost::wrapexcept<po::unknown_option>& pErr) {
    std::println("{}, use -h for help", pErr.what());
    return 1;
  }
  po::notify(vm);
  
  if (vm.count("help")) {
    std::println("Usage: vmesh OPTIONS input-path\n\nMesh voxelizer\n");
    std::cout << visibleOptions;
    return 0;
  }

  if (!vm.count("in")) {
    std::println("Missing input file path, use -h for help");
    return 1;
  }

  size_t lastIndex = in.find_last_of(".");
  out = in.substr(0, lastIndex);
  paletteOut = out;
  paletteOut = paletteOut.append(".pal");

  try {
    addColourThreshold = boost::lexical_cast<float>(addColourThresholdStr);
  }
  catch (boost::bad_lexical_cast&) {
    std::println("Invalid colourThreshold, use -h for help");
    return 1;
  }

  if (scaleMode != "proportional" && scaleMode != "stretch" && scaleMode != "none") {
    std::println("Invalid scale mode, use -h for help");
    return 1;
  }
  
  if (isSvdag && (!resolution || resolution & (resolution - 1))) {
    std::println("Octree resolution has to be a power of 2");
    return 1;
  }

  float logRes = std::log2f(resolution);
  if (subdivisionlevel > logRes) {
    std::println("Subdivision level has to be in range cannot be greater than log2(resolution) = {}", logRes);
    return 1;
  }

  if (subdivisionlevel != 0 && (!isSvdag && !isConvert)) {
    std::println("Subdivision can only be done when generating an octree");
    return 1;
  }

  if (isTribox) isBinary = true;

  // ############
  // - Generate -
  // ############

  if (isConvert && !isCompressed) std::println("Converting uncompressed Voxel Data to Sparse Voxel Octree");
  else if (isConvert && isCompressed) std::println("Converting compressed Voxel Data to Sparse Voxel Octree");
  else if (isSvdag && !isCompressed) std::println("Generating Sparse Voxel Octree of resolution {0}", resolution);
  else if (isSvdag && isCompressed)  std::println("Generating compressed Sparse Voxel Directed Acyclic Graph of resolution {0}", resolution);
  else if (isCompressed) std::println("Generating compressed Voxel Data of resolution {0}", resolution);
  else if (isConvertVox) std::println("Converting magicavoxel .vox file to svdag");
  else std::println("Generating uncompressed Voxel Data of resolution {0}", resolution);

  // Convert
  if (isConvert || isConvertVox) {
    VMesh::VoxelGrid voxelGrid(resolution);
    if (isBinary) voxelGrid.mPalette.addColour({1,1,1});
    if (isConvertVox) voxelGrid.loadFromVoxFile(in);
    else if (!isCompressed) voxelGrid.loadFromFile(in);
    else voxelGrid.loadFromFileCompressed(in);

    for (resolution = 1; resolution < voxelGrid.getResolution(); resolution <<= 1) {}

    uint64_t completedCount = 0;
    uint64_t total = std::log2f(resolution) * resolution * resolution * resolution;
    std::future<void> f = startProgressBar(&voxelGrid.mDefaultLogMutex, "Generating SVO", &completedCount, total);
    VMesh::Timer t;
    Octree svo(voxelGrid, &completedCount);
    f.wait();
    std::println("Generating SVO took: {}", t.getTime());
    if (isConvertVox) std::println("Octree resolution: {}", svo.getResolution());

    std::println("Writing pallete to: {}", paletteOut);
    voxelGrid.mPalette.writeToFile(paletteOut);

    svo.write(out);

    std::println("Complete");
    return 0;
  }

  VMesh::Model model;

  // Load mesh data
  model.load(in);

  // Get smallest and largest positions in each axis of the models vertices
  glm::vec3 smallest = glm::vec3(std::numeric_limits<float>::infinity());
  glm::vec3 largest = glm::vec3(-std::numeric_limits<float>::infinity());
  for (uint i = 0; i < model.getNumMeshes(); ++i) {
    VMesh::Mesh& m = model.getMesh(i);
    const std::vector<VMesh::Vertex>& meshVertices = m.getVertices();
    const std::vector<uint>& meshIndices = m.getIndices();
    for (uint j = 0; j < meshIndices.size(); ++j) {
      glm::vec3 v = meshVertices[meshIndices[j]].pos;
      smallest = glm::min(smallest, v);
      largest = glm::max(largest, v);
    }
  }

  // Create transformation matrix to fit model inside the grid
  glm::mat4 m(1.0f);
  if (scaleMode == "stretch")
    m = glm::scale(m, glm::vec3(resolution-1, resolution-1, resolution-1) / (largest + (glm::vec3(0, 0, 0) - smallest)) );
  else if (scaleMode == "proportional") {
    glm::vec3 vec(glm::vec3(resolution-1, resolution-1, resolution-1) / (largest + (glm::vec3(0, 0, 0) - smallest)));
    float v = std::min(vec.x, std::min(vec.y, vec.z));
    m = glm::scale(m,  glm::vec3(v, v, v));
  }
  m = glm::translate(m, glm::vec3(0, 0, 0) - smallest);
  
  for (uint i = 0; i < model.getNumMeshes(); ++i)
    model.getMesh(i).transformVertices(m);

  // Generate
  if (isSvdag) {
    uint subdivisionSize = resolution >> subdivisionlevel;
    uint numSubdivisions = resolution / subdivisionSize;
    numSubdivisions = numSubdivisions * numSubdivisions * numSubdivisions;
    uint subdimensions = 1 << subdivisionlevel;

    Octree parentSVO(resolution, glm::ceil(3.f / addColourThreshold));
    
    std::chrono::duration<double> totalVoxelizationTime, totalOctreeGenerationTime, temp;

    VMesh::VoxelGrid grid(subdivisionSize);
    if (isTribox || isBinary) grid.mPalette.addColour({1,1,1});
    if (isVerbose) grid.setLogStream(&std::cout);

    uint subdivision = 0;
    for (glm::uvec3 o(0); o.x < subdimensions; ++o.x) for (o.y = 0; o.y < subdimensions; ++o.y) for (o.z = 0; o.z < subdimensions; ++o.z, ++subdivision) {
      VMesh::Timer t;
      std::println("Subdivision: {}/{}", subdivision + 1, numSubdivisions);

      grid.clear();
      glm::uvec3 origin(o.x * subdivisionSize, o.y * subdivisionSize, o.z * subdivisionSize);
      grid.setOrigin(origin);

      uint64_t trisComplete = 0;
      std::future<void> f = startProgressBar(&grid.mDefaultLogMutex, "Voxelizing", &trisComplete, model.getTriCount());

      if (!isTribox) grid.DDAvoxelizeModel(model, reinterpret_cast<uint*>(&trisComplete), !isBinary, addColourThreshold);
      else           grid.IntersectVoxelizeModel(model, reinterpret_cast<uint*>(&trisComplete));

      temp = t.getTime();
      totalVoxelizationTime += temp;

      if (!grid.getVoxelCount()) {
        f.wait();
        std::println("Subdivision: {}/{} took {}", subdivision + 1, numSubdivisions, t.getTime());
        continue;
      }

      grid.setOrigin();

      uint64_t completedCount = 0;
      uint64_t total = grid.getMaxDepth() * grid.getVolume();
      f = startProgressBar(&grid.mDefaultLogMutex, "Generating SVO", &completedCount, total);
      Octree svo(grid, &completedCount);
      f.wait();
      parentSVO.attach(svo, origin);

      totalOctreeGenerationTime += t.getTime() - temp;

      std::println("Subdivision: {}/{} took {}", subdivision + 1, numSubdivisions, t.getTime());
    }
    parentSVO.resizePalette(grid.mPalette.size());

    std::println("----------------------------------");
    std::println("Total voxelization time: {}", totalVoxelizationTime);
    std::println("Total octree generation time: {}", totalOctreeGenerationTime);
    std::println("----------------------------------");

    std::println("Writing pallete to: {}", paletteOut);
    grid.mPalette.writeToFile(paletteOut);

    parentSVO.write(out);
    
    std::println("Complete");
    model.release();
    return 0;
  }

  // Set log stream
  VMesh::VoxelGrid voxelGrid(resolution);

  if (isVerbose) voxelGrid.setLogStream(&std::cout);
  if (isTribox || isBinary) voxelGrid.mPalette.addColour({1,1,1});

  // Voxelize
  uint64_t trisComplete = 0;
  std::future<void> f = startProgressBar(&voxelGrid.mDefaultLogMutex, "Voxelizing", &trisComplete, model.getTriCount());
  if (!isTribox) voxelGrid.DDAvoxelizeModel(model, reinterpret_cast<uint*>(&trisComplete), !isBinary, addColourThreshold);
  else voxelGrid.IntersectVoxelizeModel(model, reinterpret_cast<uint*>(&trisComplete));

  f.wait();

  std::println("Writing pallete to: {}", paletteOut);
  voxelGrid.mPalette.writeToFile(paletteOut);

  // TODO: Sort out writing for binary and non binary files for non octrees

  out.append(".bin");
  std::println("Writing voxel grid to: {}", out);

  // Uncompressed and not svdag
  if (!isCompressed && !isSvdag) voxelGrid.writeToFile(out);
  else if (isCompressed && !isSvdag) { // Compressed and not svdag
    uint64_t voxelsComplete = 0;
    std::future<void> f = startProgressBar(&voxelGrid.mDefaultLogMutex, "Compressing", &voxelsComplete, voxelGrid.getVolume());
    voxelGrid.writeToFileCompressed(out, &voxelsComplete);
  }

  std::println("Complete");
  model.release();
  return 0;
}

