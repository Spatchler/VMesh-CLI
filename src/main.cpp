#include "VMesh/model.hpp"
#include "VMesh/voxelGrid.hpp"

#include "progressBar.hpp"

#include <array>
#include <vector>
#include <boost/program_options.hpp>
#include <iostream>
#include <fstream>

namespace po = boost::program_options;

glm::vec3 toPos(uint pChildIndex);

uint generateSVDAGTopDown(std::vector<std::array<uint, 8>>& pIndices, VMesh::VoxelGrid& pGrid, glm::vec3 pNodeOrigin, uint pNodeSize, std::vector<std::tuple<uint, uint, glm::vec3, uint>>& pQueue, uint64_t& pCompletedCount);

void writeSVDAG(VMesh::VoxelGrid& pGrid, const std::string& pOut);

int main(int argc, char** argv) {
  uint resolution;
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
    ("scale-mode", po::value<std::string>(&scaleMode)->default_value("proportional"), "scaling mode either (proportional, stretch, none)")
    ("voxel-to-svdag", "input voxel binary file and output svdag")
    ("DDA", "use DDA voxelization algorithm instead of triangle box intersections")
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

  {
    float logRes = std::log2f(resolution);
    if (isSvdag && logRes != std::ceil(logRes) && logRes != std::floor(logRes)) {
      std::println("Octree resolution has to be a power of 2");
      return 1;
    }
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

  // Set log stream
  VMesh::VoxelGrid voxelGrid(resolution);

  if (isVerbose)
    voxelGrid.setLogStream(&std::cout);

  // Convert
  if (isConvert) {
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
  uint64_t trisComplete = 0;
  std::future<void> f = startProgressBar(&voxelGrid.mDefaultLogMutex, "Voxelizing", &trisComplete, model.getTriCount());
  if (isDDA)
    voxelGrid.voxelizeMesh(model, reinterpret_cast<uint*>(&trisComplete));
  else
    voxelGrid.DDAvoxelizeMesh(model, reinterpret_cast<uint*>(&trisComplete));

  // #########
  // - Write -
  // #########

  if (!isCompressed && !isSvdag) // Uncompressed and not svdag
    voxelGrid.writeToFile(out);
  else if (isCompressed && !isSvdag) { // Compressed and not svdag
    uint64_t voxelsComplete = 0;
    std::future<void> f = startProgressBar(&voxelGrid.mDefaultLogMutex, "Compressing", &voxelsComplete, voxelGrid.getVolume());
    voxelGrid.writeToFileCompressed(out, &voxelsComplete);
  }

  else if (isSvdag)
    writeSVDAG(voxelGrid, out);

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

uint generateSVDAGTopDown(std::vector<std::array<uint, 8>>& pIndices, VMesh::VoxelGrid& pGrid, glm::vec3 pNodeOrigin, uint pNodeSize, std::vector<std::tuple<uint, uint, glm::vec3, uint>>& pQueue, uint64_t& pCompletedCount) {
  bool allZero = true;
  bool allOne = true;
  uint64_t volume = static_cast<uint64_t>(pNodeSize) * pNodeSize * pNodeSize;
  uint64_t total = pCompletedCount + volume;

  for (uint x = pNodeOrigin.x; x < pNodeOrigin.x + pNodeSize; ++x)
    for (uint y = pNodeOrigin.y; y < pNodeOrigin.y + pNodeSize; ++y)
      for (uint z = pNodeOrigin.z; z < pNodeOrigin.z + pNodeSize && (allOne || allZero); ++z,++pCompletedCount) {
        // const bool& v = pVoxelData[x][y][z];
        const bool v = pGrid.queryVoxel(glm::vec3(x, y, z));
        allZero = allZero && v == 0;
        allOne = allOne && v == 1;
      }

  pCompletedCount = total;

  if (allZero || allOne)
    pCompletedCount += std::log2(pNodeSize) * volume;
  if (allZero)
    return 0;
  else if (allOne)
    return 1;

  pNodeSize = pNodeSize >> 1;

  pIndices.emplace_back();
  for (uint i = 0; i < 8; ++i)
    pQueue.push_back({pIndices.size()-1, i, pNodeOrigin + (float)pNodeSize * toPos(i), pNodeSize});

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
  std::vector<std::tuple<uint, uint, glm::vec3, uint>> queue;
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
    indices.at(std::get<0>(queueItem)).at(std::get<1>(queueItem)) = generateSVDAGTopDown(indices, pGrid, std::get<2>(queueItem), std::get<3>(queueItem), queue, completedCount);
    if (std::get<0>(queueItem) >= UINT_MAX - 2) {
      std::lock_guard<std::mutex> lock(pGrid.mDefaultLogMutex);
      std::println("Indices overflow");
    }
    ++queueIndex;
    if (queueIndex >= 10000000) {
      {
        std::lock_guard<std::mutex> lock(pGrid.mDefaultLogMutex);
        // Queue item size: 24 bytes
        // 24 * 10000000 / 1024 / 1024 = 228
        std::println("Clearing 228mb recursion stack", sizeof(queue.at(0)) * queueIndex);
      }
      queue.erase(queue.begin(), queue.begin() + queueIndex);
      queueIndex = 0;
    }
  }

  {
    std::lock_guard<std::mutex> lock(pGrid.mDefaultLogMutex);
    std::println("Writing");
  }

  fout.write(reinterpret_cast<char*>(&indices.at(0)), indices.size() * 8 * sizeof(uint32_t));

  fout.close();
}

