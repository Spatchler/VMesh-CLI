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
  bool isVerbose, isTribox, isBinary, isCreatePalette;
  float addColourDistance2;
  std::string in, out, outputFormat, palettePath, scaleMode, addColourDistanceStr;

  // VMesh::Palette testPalette;
  // for (uint r = 0; r <= 255; ++r) {
  //   for (uint g = 0; g <= 255; ++g) for (uint b = 0; b <= 255; ++b)
  //     testPalette.addColour({r / 255.f, g / 255.f, b / 255.f}, 0.1f);
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
    ("format,f", po::value<std::string>(&outputFormat), "specify output format (vmu, vmc, vm8, vm64)")
    ("palette,P", po::value<std::string>(&palettePath), "specify path to an existing palette to use rather than create one")
    ("resolution,R", po::value<uint>(&resolution)->default_value(128), "set voxel grid resolution")
    ("subdivision-level,L", po::value<uint>(&subdivisionlevel)->default_value(0), "set depth to generate initial subtrees before combining for out of core generation")
    ("scale-mode", po::value<std::string>(&scaleMode)->default_value("proportional"), "scaling mode either (proportional, stretch, none)")
    ("tribox", po::bool_switch(&isTribox), "use triangle box intersections instead of DDA voxelization, it tends to be faster on low resolutions(<512) however it only generates binary data")
    ("binary,B", po::bool_switch(&isBinary), "generate binary voxel data instead of coloured voxel data")
    ("colour-distance", po::value<std::string>(&addColourDistanceStr)->default_value("0.1"), "set the euclidean distance between two normalized rgb colours that is required for a new colour to be added to the palette")
  ;

  po::options_description hiddenOptions("Hidden");
  hiddenOptions.add_options()
    ("in", po::value<std::string>(&in), "input file path")
    ("out", po::value<std::string>(&out), "output file path dont specify to use input path as output")
  ;

  po::positional_options_description p;
  p.add("in", 1);
  p.add("out", 1);

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
  
  // Help
  if (vm.count("help")) {
    std::println("Usage: vmesh OPTIONS input-path output-path(optional)\n\nMesh voxelizer\n");
    std::cout << visibleOptions;
    return 0;
  }

  // Input path
  if (!vm.count("in")) {
    std::println("Missing input file path, use -h for help");
    return 1;
  }

  // Output format
  if (!vm.count("out")) {
    size_t lastIndex = in.find_last_of(".");
    out = in.substr(0, lastIndex);
  }
  std::string paletteOut = out;
  paletteOut = paletteOut.append(".pal");
  if (outputFormat != "vmu" && outputFormat != "vmc" && outputFormat != "vm8" && outputFormat != "vm64") {
    std::println("Invalid output format, use -h for help");
    return 1;
  }

  // Is create palette
  isCreatePalette = !vm.count("palette");

  // Colour distance
  try {
    addColourDistance2 = boost::lexical_cast<float>(addColourDistanceStr);
    addColourDistance2 *= addColourDistance2;
  }
  catch (boost::bad_lexical_cast&) {
    std::println("Invalid colour distance, use -h for help");
    return 1;
  }

  // Scale mode
  if (scaleMode != "proportional" && scaleMode != "stretch" && scaleMode != "none") {
    std::println("Invalid scale mode, use -h for help");
    return 1;
  }

  // Temp
  if (outputFormat == "vm64") {
    std::println("64trees are not supported yet");
    return 1;
  }
  
  // Resolution
  if (outputFormat == "vm8" && (!resolution || resolution & (resolution - 1))) {
    std::println("Octree resolution has to be a power of 2");
    return 1;
  }

  // Subdivision level
  float logRes = std::log2f(resolution);
  if (subdivisionlevel > logRes) {
    std::println("Subdivision level has to be in range 0,log2(resolution)={} inclusive", logRes);
    return 1;
  }

  if ((subdivisionlevel != 0) && (outputFormat != "vm8")) {
    std::println("Out of core generation is currently only supported for octrees");
    return 1;
  }

  if (isTribox) isBinary = true;

  // ############
  // - Generate -
  // ############

  // Check file magic number
  bool isConvertC = false, isConvertU = false, isConvertVox = false;
  std::ifstream fin;
  fin.open(in, std::ios::binary | std::ios::in);
  if (!fin.is_open()) throw std::invalid_argument("Could not open input file");
  // Magica voxel
  {
    uint32_t magicNumber;
    fin.read(reinterpret_cast<char*>(&magicNumber), 4);
    fin.seekg(0);
    isConvertVox = magicNumber == *reinterpret_cast<const uint32_t*>("VOX ");
  }
  // vmu
  if (!isConvertVox) {
    std::string str;
    str.resize(6);
    fin.read(str.data(), 6);
    fin.seekg(0);
    isConvertU = str == "VMESHU";
    // vmc
    if (!isConvertU) {
      fin.read(str.data(), 6);
      isConvertC = str == "VMESHC";
    }
  }
  fin.close();

  // Print what is going on
  std::string outputFormatFull;
  if (outputFormat == "vmu") outputFormatFull = "uncompressed voxel data";
  else if (outputFormat == "vmc") outputFormatFull = "compressed voxel data";
  else if (outputFormat == "vm8") outputFormatFull = "sparse voxel octree";
  else if (outputFormat == "vm64") outputFormatFull = "sparse voxel 64tree";

  if (isConvertVox) std::println("Converting magica voxel file to {}", outputFormatFull);
  else if (isConvertU) std::println("Converting uncompressed voxel data to {}", outputFormatFull);
  else if (isConvertC) std::println("Converting compressed voxel data to {}", outputFormatFull);
  else std::println("Generating {} from 3d model", outputFormatFull);

  // Convert
  if (isConvertVox || isConvertU || isConvertC) {
    VMesh::VoxelGrid voxelGrid(resolution);
    // if (isBinary) voxelGrid.mPalette.addColour({1,1,1});
    if (isConvertVox) voxelGrid.loadFromVoxFile(in);
    else if (isConvertU) voxelGrid.loadFromFile(in);
    else voxelGrid.loadFromFileCompressed(in);

    if (isConvertVox) {
      std::println("Writing pallete to: \e[1;3;4;33m{}\e[0m", paletteOut);
      voxelGrid.mPalette.writeToFile(paletteOut);
    }

    if (outputFormat == "vmu") {
      voxelGrid.writeToFile(out);
      std::println("Complete");
      return 0;
    }
    if (outputFormat == "vmc") {
      uint64_t voxelsComplete = 0;
      std::future<void> f = startProgressBar(&voxelGrid.mDefaultLogMutex, "Compressing", &voxelsComplete, voxelGrid.getVolume());
      voxelGrid.writeToFileCompressed(out, &voxelsComplete);
      std::println("Complete");
      return 0;
    }

    for (resolution = 1; resolution < voxelGrid.getResolution(); resolution <<= 1) {}

    uint64_t completedCount = 0;
    uint64_t total = std::log2f(resolution) * resolution * resolution * resolution;
    std::future<void> f = startProgressBar(&voxelGrid.mDefaultLogMutex, "Generating SVO", &completedCount, total);
    VMesh::Timer t;
    Octree svo(voxelGrid, &completedCount);
    f.wait();
    std::println("Generating SVO took: {}", t.getTime());

    if (isConvertVox) std::println("Octree resolution: {}", svo.getResolution());

    svo.write(out);

    std::println("Complete");
    return 0;
  }

  // Load model
  VMesh::Model model;
  {
    std::println("Loading model...");
    VMesh::Timer t;
    model.load(in);
    std::println("Loading model took: {}", t.getTime());
  }

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
  if (outputFormat == "vm8") {
    uint subdivisionSize = resolution >> subdivisionlevel;
    uint numSubdivisions = resolution / subdivisionSize;
    numSubdivisions = numSubdivisions * numSubdivisions * numSubdivisions;
    uint subdimensions = 1 << subdivisionlevel;
    
    std::chrono::duration<double> totalVoxelizationTime, totalOctreeGenerationTime, temp;

    VMesh::VoxelGrid grid(subdivisionSize);
    if (isTribox || isBinary) grid.mPalette.addColour({1,1,1});
    if (!isCreatePalette) grid.mPalette.readFromFile(palettePath);
    if (isVerbose) grid.setLogStream(&std::cout);

    Octree parentSVO(resolution, isCreatePalette ? 255 : grid.mPalette.size());

    uint subdivision = 0;
    for (glm::uvec3 o(0); o.x < subdimensions; ++o.x) for (o.y = 0; o.y < subdimensions; ++o.y) for (o.z = 0; o.z < subdimensions; ++o.z, ++subdivision) {
      VMesh::Timer t;
      std::println("Subdivision: {}/{}", subdivision + 1, numSubdivisions);

      grid.clear();
      glm::uvec3 origin(o.x * subdivisionSize, o.y * subdivisionSize, o.z * subdivisionSize);
      grid.setOrigin(origin);

      uint64_t trisComplete = 0;
      std::future<void> f = startProgressBar(&grid.mDefaultLogMutex, "Voxelizing", &trisComplete, model.getTriCount());

      if (!isTribox) grid.DDAvoxelizeModel(model, reinterpret_cast<uint*>(&trisComplete), !isBinary, isCreatePalette, addColourDistance2);
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
    std::println("----------------------------------");
    std::println("Total voxelization time: {}", totalVoxelizationTime);
    std::println("Total octree generation time: {}", totalOctreeGenerationTime);
    std::println("----------------------------------");

    if (isCreatePalette) {
      std::println("palsize: {}", grid.mPalette.size());
      if (grid.mPalette.size() > 255) throw std::runtime_error("Max palette size is 255, try increasing colour-distance");
      parentSVO.resizePalette(grid.mPalette.size());
      std::println("Writing pallete to: \e[1;3;4;33m{}\e[0m", paletteOut);
      grid.mPalette.writeToFile(paletteOut);
    }

    parentSVO.write(out);
    
    std::println("Complete");
    model.release();
    return 0;
  }

  // Set log stream
  VMesh::VoxelGrid voxelGrid(resolution);

  if (isVerbose) voxelGrid.setLogStream(&std::cout);
  if (isTribox || isBinary) voxelGrid.mPalette.addColour({1,1,1});
  if (!isCreatePalette) voxelGrid.mPalette.readFromFile(palettePath);

  // Voxelize
  uint64_t trisComplete = 0;
  std::future<void> f = startProgressBar(&voxelGrid.mDefaultLogMutex, "Voxelizing", &trisComplete, model.getTriCount());
  if (!isTribox) voxelGrid.DDAvoxelizeModel(model, reinterpret_cast<uint*>(&trisComplete), !isBinary, isCreatePalette, addColourDistance2);
  else voxelGrid.IntersectVoxelizeModel(model, reinterpret_cast<uint*>(&trisComplete));

  f.wait();

  if (isCreatePalette) {
    std::println("Writing pallete to: \e[1;3;4;33m{}\e[0m", paletteOut);
    voxelGrid.mPalette.writeToFile(paletteOut);
  }

  if (outputFormat == "vmu") voxelGrid.writeToFile(out);
  else {
    uint64_t voxelsComplete = 0;
    std::future<void> f = startProgressBar(&voxelGrid.mDefaultLogMutex, "Compressing", &voxelsComplete, voxelGrid.getVolume());
    voxelGrid.writeToFileCompressed(out, &voxelsComplete);
  }

  std::println("Complete");
  model.release();
  return 0;
}

