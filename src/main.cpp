




// Add proper pallete support to octree files







#include "VMesh/model.hpp"
#include "VMesh/voxelGrid.hpp"

#include "progressBar.hpp"
#include "octree.hpp"

#include <array>
#include <vector>
#include <boost/program_options.hpp>
#include <iostream>
#include <fstream>

namespace po = boost::program_options;

glm::vec3 toPos(uint pChildIndex);

uint generateSVDAGTopDown(std::vector<std::array<uint, 8>>& pIndices, VMesh::VoxelGrid& pGrid, glm::vec3 pNodeOrigin, uint pNodeSize, std::vector<std::tuple<uint, glm::vec3, uint>>& pQueue, uint64_t& pCompletedCount);

uint generateOctreeToDepth(std::vector<std::array<uint32_t, 8>>& pIndices, std::vector<std::tuple<uint, uint>>& pQueue, uint pCurrentDepth);

void writeSVDAG(VMesh::VoxelGrid& pGrid, const std::string& pOut);

int main(int argc, char** argv) {
  uint resolution;
  uint subdivisionlevel;
  bool isSvdag = false;
  bool isVerbose = false;
  bool isCompressed = false;
  bool isConvert = false;
  bool isDDA = false;
  std::string in, out, scaleMode, inputType;
  
  // ##############
  // - Parse args -
  // ##############

  po::options_description visibleOptions("Options", 100, 40);
  visibleOptions.add_options()
    ("help,h", "produce help message")
    ("verbose,v", "verbose output")
    ("compressed,C", "compress voxel data")
    ("svdag,S", "generate a Sparse Voxel Octree instead of a normal voxel grid")
    ("resolution,R", po::value<uint>(&resolution)->default_value(128), "set voxel grid resolution")
    ("subdivision-level,L", po::value<uint>(&subdivisionlevel)->default_value(0), "set depth to generate initial subtrees before combining")
    ("scale-mode", po::value<std::string>(&scaleMode)->default_value("proportional"), "scaling mode either (proportional, stretch, none)")
    ("voxel-to-svdag", "input voxel binary file and output svdag")
    ("DDA", "use DDA voxelization algorithm instead of triangle box intersections, it tends to be faster on higher resolutions")
  ;

  po::options_description hiddenOptions("Hidden");
  hiddenOptions.add_options()
    ("in", po::value<std::string>(&in), "input file path")
    ("out", po::value<std::string>(&out), "output file path")
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
  catch (boost::wrapexcept<po::invalid_option_value> pErr) {
    std::println("{}, use -h for help", pErr.what());
    return 1;
  }
  catch (boost::wrapexcept<po::unknown_option> pErr) {
    std::println("{}, use -h for help", pErr.what());
    return 1;
  }
  po::notify(vm);
  
  if (vm.count("help")) {
    std::println("Usage: vmesh OPTIONS input-path output-path\n\nMesh voxelizer\n");
    std::cout << visibleOptions;
    return 0;
  }

  if (vm.count("verbose"))
    isVerbose = true;
  if (vm.count("compressed"))
    isCompressed = true;

  if (!vm.count("in")) {
    std::println("Missing input file path, use -h for help");
    return 1;
  }
  if (!vm.count("out")) {
    std::println("Missing output file path, use -h for help");
    return 1;
  }

  if (vm.count("svdag"))
    isSvdag = true;

  if (scaleMode != "proportional" && scaleMode != "stretch" && scaleMode != "none") {
    std::println("Invalid scale mode, use -h for help");
    return 1;
  }

  if (vm.count("voxel-to-svdag"))
    isConvert = true;

  if (vm.count("DDA"))
    isDDA = true;

  float logRes = std::log2f(resolution);
  if (isSvdag && logRes != std::ceil(logRes) && logRes != std::floor(logRes)) {
    std::println("Octree resolution has to be a power of 2");
    return 1;
  }

  if (subdivisionlevel > logRes) {
    std::println("Subdivision level has to be in range cannot be greater than log2(resolution) = {}", logRes);
    return 1;
  }

  if (subdivisionlevel != 0 && (!isSvdag && !isConvert)) {
    std::println("Subdivision can only be done when generating an octree");
    return 1;
  }

  // ############
  // - Generate -
  // ############

  std::string compressedStr = "uncompressed";
  if (isCompressed)
    compressedStr = "compressed";

  if (isConvert)
    std::println("Converting uncompressed Voxel Data to Sparse Voxel Octree");
  else if (isSvdag)
    std::println("Generating {0} Sparse Voxel Octree of resolution {1}", compressedStr, resolution);
  else
    std::println("Generating {0} Voxel Data of resolution {1}", compressedStr, resolution);

  // Convert
  if (isConvert) {
    VMesh::VoxelGrid voxelGrid(resolution);

    voxelGrid.loadFromFile(in);

    resolution = voxelGrid.getResolution();

    writeSVDAG(voxelGrid, out);

    std::println("Complete");
    return 0;
  }

  VMesh::Model model;

  // Load mesh data
  try {
    model.load(in);
  }
  catch (std::string pErr) {
    std::println("{}", pErr);
    return 1;
  }

  // Get smallest and largest positions in each axis of the models vertices
  const std::vector<glm::vec3>& meshVertices = model.getVertices();
  const std::vector<uint>& meshIndices = model.getIndices();
  glm::vec3 smallest = meshVertices.at(0);
  glm::vec3 largest = meshVertices.at(0);
  for (uint i = 0; i < meshIndices.size(); ++i) {
    glm::vec3 v = meshVertices[meshIndices[i]];
    smallest = glm::min(smallest, v);
    largest = glm::max(largest, v);
  }

  // Create transformation matrix to fit model perfectly inside the grid
  glm::mat4 m(1.0f);
  if (scaleMode == "stretch")
    m = glm::scale(m, glm::vec3(resolution-1, resolution-1, resolution-1) / (largest + (glm::vec3(0, 0, 0) - smallest)) );
  if (scaleMode == "proportional") {
    glm::vec3 vec(glm::vec3(resolution-1, resolution-1, resolution-1) / (largest + (glm::vec3(0, 0, 0) - smallest)));
    float v = std::min(vec.x, std::min(vec.y, vec.z));
    m = glm::scale(m,  glm::vec3(v, v, v));
  }
  m = glm::translate(m, glm::vec3(0, 0, 0) - smallest);
  
  model.transformVertices(m);

  // Generate
  if (isSvdag) {
    uint subdivisionSize = resolution >> subdivisionlevel;
    uint numSubdivisions = resolution / subdivisionSize;
    numSubdivisions = numSubdivisions * numSubdivisions * numSubdivisions;
    uint subdimensions = 1 << subdivisionlevel;

    SparseVoxelOctree parentSVO(resolution);

    uint subdivision = 0;
    for (glm::uvec3 o(0); o.x < subdimensions; ++o.x) for (o.y = 0; o.y < subdimensions; ++o.y) for (o.z = 0; o.z < subdimensions; ++o.z, ++subdivision) {
      VMesh::Timer t;
      std::println("Subdivision: {}/{}", subdivision + 1, numSubdivisions);

      VMesh::VoxelGrid grid(subdivisionSize);
      glm::vec3 origin = glm::vec3(o.x * subdivisionSize, o.y * subdivisionSize, o.z * subdivisionSize);
      grid.setOrigin(origin);

      uint64_t trisComplete = 0;
      std::future<void> f = startProgressBar(&grid.mDefaultLogMutex, "Voxelizing", &trisComplete, model.getTriCount());

      if (isDDA) grid.DDAvoxelizeMesh(model, reinterpret_cast<uint*>(&trisComplete));
      else       grid.voxelizeMesh(model, reinterpret_cast<uint*>(&trisComplete));

      grid.setOrigin();

      uint64_t completedCount = 0;
      uint64_t total = grid.getMaxDepth() * grid.getVolume();
      f = startProgressBar(&grid.mDefaultLogMutex, "Generating SVDAG", &completedCount, total);
      SparseVoxelOctree svo(grid, &completedCount);
      f.wait();
      parentSVO.attachSVO(svo, origin);

      std::println("Subdivision: {}/{} took {}", subdivision + 1, numSubdivisions, t.getTime());
    }

    std::println("Generating indices");
    std::vector<std::array<uint32_t, 8>> indices = parentSVO.generateIndices();

    std::println("Writing");
    std::ofstream fout;
    fout.open(out, std::ios::out | std::ios::binary);
    if (!fout.is_open()) throw "Could not open output file";

    fout.write(reinterpret_cast<char*>(&resolution), sizeof(uint32_t));

    uint32_t indicesSize = indices.size();
    fout.write(reinterpret_cast<char*>(&indicesSize), sizeof(uint32_t));
    fout.write(reinterpret_cast<char*>(&indices.at(0)), indices.size() * 8 * sizeof(uint32_t));

    fout.close();
    
    std::println("Complete");
    return 0;
  }

  // Set log stream
  VMesh::VoxelGrid voxelGrid(resolution);

  if (isVerbose)
    voxelGrid.setLogStream(&std::cout);

  // Voxelize
  uint64_t trisComplete = 0;
  if (isDDA)
    voxelGrid.DDAvoxelizeMesh(model, reinterpret_cast<uint*>(&trisComplete));
  else
    voxelGrid.voxelizeMesh(model, reinterpret_cast<uint*>(&trisComplete));

  if (!isCompressed && !isSvdag) // Uncompressed and not svdag
    voxelGrid.writeToFile(out);
  else if (isCompressed && !isSvdag) { // Compressed and not svdag
    uint64_t voxelsComplete = 0;
    std::future<void> f = startProgressBar(&voxelGrid.mDefaultLogMutex, "Compressing", &voxelsComplete, voxelGrid.getVolume());
    voxelGrid.writeToFileCompressed(out, &voxelsComplete);
  }

  std::println("Complete");
  return 0;
}

glm::vec3 toPos(uint pChildIndex) {
  glm::vec3 pos;
  pos.x = 1 & pChildIndex;
  pos.y = ((1 << 1) & pChildIndex) != 0;
  pos.z = ((1 << 2) & pChildIndex) != 0;
  return pos;
}

uint generateSVDAGTopDown(std::vector<std::array<uint32_t, 8>>& pIndices, VMesh::VoxelGrid& pGrid, glm::vec3 pNodeOrigin, uint pNodeSize, std::vector<std::tuple<uint, glm::vec3, uint>>& pQueue, uint64_t& pCompletedCount) {
  bool allZero = true;
  bool allOne = true;
  uint64_t volume = static_cast<uint64_t>(pNodeSize) * pNodeSize * pNodeSize;
  uint64_t total = pCompletedCount + volume;

  for (uint z = pNodeOrigin.z; z < pNodeOrigin.z + pNodeSize; ++z)
    for (uint y = pNodeOrigin.y; y < pNodeOrigin.y + pNodeSize; ++y)
      for (uint x = pNodeOrigin.x; x < pNodeOrigin.x + pNodeSize && (allOne || allZero); ++x,++pCompletedCount) {
        // const bool& v = pVoxelData[x][y][z];
        const bool v = pGrid.queryVoxel(glm::vec3(x, y, z));
        allZero = allZero && v == 0;
        allOne = allOne && v == 1;
      }

  pCompletedCount = total;

  if (allZero || allOne)
    pCompletedCount += std::log2(pNodeSize) * volume;

  if (allZero) return 0;
  if (allOne)  return 1;

  pNodeSize = pNodeSize >> 1;

  pIndices.emplace_back();
  for (uint i = 0; i < 8; ++i)
    pQueue.push_back({pIndices.size()-1 + i, pNodeOrigin + (float)pNodeSize * toPos(i), pNodeSize});

  return pIndices.size() + 1;
}

uint generateOctreeToDepth(std::vector<std::array<uint32_t, 8>>& pIndices, std::vector<std::tuple<uint, uint>>& pQueue, uint pCurrentDepth) {
  ++pCurrentDepth;
  pIndices.emplace_back();
  for (uint i = 0; i < 8; ++i)
    pIndices.back()[i] = pIndices.size() + i;
  for (uint i = 0; i < 8; ++i)
    pQueue.push_back({pIndices.size()-1 + i, pCurrentDepth});
  return pIndices.size() + 1;
}

void writeSVDAG(VMesh::VoxelGrid& pGrid, const std::string& pPath) {
  std::ofstream fout;

  fout.open(pPath, std::ios::out | std::ios::binary);
  if (!fout.is_open())
    throw "Could not open output file";

  uint res = pGrid.getResolution();

  // Write resolution metadata
  fout.write(reinterpret_cast<char*>(&res), sizeof(uint32_t));

  // SparseVoxelDAG svdag(resolution);
  // const std::vector<std::vector<std::vector<bool>>>& voxelData = pGrid.getVoxelData();
  // const std::vector<char>& voxelData = voxelGrid.getVoxelDataBytes();

  std::vector<std::array<uint32_t, 8>> indices;
  std::vector<std::tuple<uint, glm::vec3, uint>> queue;
  uint64_t completedCount = 0;
  uint64_t total = (pGrid.getMaxDepth() + 1) * pGrid.getVolume();
  float totalInv = 1.f/total;

  std::future<void> f = startProgressBar(&pGrid.mDefaultLogMutex, "Generating SVDAG", &completedCount, total);

  {
    uint i = generateSVDAGTopDown(indices, pGrid, glm::vec3(0, 0, 0), res, queue, completedCount);
    if (i <= 1)
      indices.push_back({i, i, i, i, i, i, i, i});
  }
  uint queueIndex = 0;

  while (queueIndex < queue.size()) {
    auto queueItem = queue.at(queueIndex);
    indices.at(std::get<0>(queueItem) >> 3).at(std::get<0>(queueItem) & 0b111) = generateSVDAGTopDown(indices, pGrid, std::get<1>(queueItem), std::get<2>(queueItem), queue, completedCount);
    if (std::get<0>(queueItem) >= UINT_MAX) {
      std::lock_guard<std::mutex> lock(pGrid.mDefaultLogMutex);
      std::println("\r\e2KIndices overflow");
    }
    ++queueIndex;
    // if (queueIndex >= 10000000) { // 228mb
    if (queueIndex >= 4369067) { // 100mb
      {
        std::lock_guard<std::mutex> lock(pGrid.mDefaultLogMutex);
        // Queue item size: 24 bytes
        // 24 * 4369067 / 1024 / 1024 = 100
        std::println("\r\e[2KClearing 100mb recursion stack", sizeof(queue.at(0)) * queueIndex);
      }
      queue.erase(queue.begin(), queue.begin() + queueIndex);
      queueIndex = 0;
    }
  }

  f.wait();
  std::println("Writing");

  fout.write(reinterpret_cast<char*>(&indices.at(0)), indices.size() * 8 * sizeof(uint32_t));

  fout.close();
}

