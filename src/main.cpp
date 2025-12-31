#include "VMesh/model.hpp"
#include "VMesh/voxelGrid.hpp"

// #include "SparseVoxelDAG.hpp"
#include "progressBar.hpp"

#include <array>
#include <vector>
#include <boost/program_options.hpp>
#include <iostream>
#include <fstream>

namespace po = boost::program_options;

uint generateSVDAGTopDown(std::vector<std::array<uint, 8>>& pIndices, const std::vector<std::vector<std::vector<bool>>>& pVoxelData, glm::vec3 pNodeOrigin, uint pNodeSize, std::vector<std::tuple<uint, uint, glm::vec3, uint>>& pQueue, uint& pCompletedCount);

uint generateSVDAGBottomUp(std::vector<std::array<uint, 8>>& pIndices, const std::vector<std::vector<std::vector<bool>>>& pVoxelData, glm::vec3 pNodeOrigin, uint pNodeSize, std::vector<std::tuple<uint, uint, glm::vec3, uint>>& pQueue, uint& pCompletedCount);

int main(int argc, char** argv) {
  uint resolution;
  bool isSvdag = false;
  bool isVerbose = false;
  bool isCompressed = false;
  std::string in, out, scaleMode;
  
  // ##############
  // - Parse args -
  // ##############

  po::options_description visibleOptions("Options", 100, 40);
  visibleOptions.add_options()
    ("help,h", "produce help message")
    ("verbose,v", "verbose output")
    ("compressed,C", "compress voxel data")
    ("svdag,S", "generate a Sparse Voxel DAG instead of a normal voxel grid")
    ("resolution,R", po::value<uint>(&resolution)->default_value(128), "set voxel grid resolution")
    ("scale-mode", po::value<std::string>(&scaleMode)->default_value("proportional"), "scaling mode either (proportional, stretch, none)")
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

  float logRes = std::log2f(resolution);
  if (isSvdag && logRes != std::ceil(logRes) && logRes != std::floor(logRes)) {
    std::println("SVDAG resolution has to be a power of 2");
    return 1;
  }

  // ############
  // - Generate -
  // ############

  VMesh::Model model;

  // Set insert function if generating an svdag
  std::string compressedStr = "uncompressed";
  if (isCompressed)
    compressedStr = "compressed";

  if (isSvdag)
    std::println("Generating {0} Sparse Voxel DAG of resolution {1}", compressedStr, resolution);
  else
    std::println("Generating {0} Voxel Data of resolution {1}", compressedStr, resolution);

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

  // Set log stream
  VMesh::VoxelGrid voxelGrid(resolution);

  std::stringstream stream;
  voxelGrid.setLogStream(&stream);
  if (isVerbose)
    voxelGrid.setLogStream(&std::cout);

  // Generate
  uint trisComplete = 0;
  float triCountInv = 1.f/model.getTriCount();
  std::future<void> f = startProgressBar(&voxelGrid.mDefaultLogMutex, [&trisComplete, &triCountInv](){ return trisComplete * triCountInv; }, "Voxelizing");
  voxelGrid.voxelizeMesh(model, &trisComplete);

  // #########
  // - Write -
  // #########

  if (!isSvdag) {
    std::lock_guard<std::mutex> lock(voxelGrid.mDefaultLogMutex);
    std::println("Writing...");
  }

  if (!isCompressed && !isSvdag) // Uncompressed and not svdag
    voxelGrid.writeToFile(out);
  else if (isCompressed && !isSvdag) // Compressed and not svdag
    voxelGrid.writeToFileCompressed(out);

  else if (isSvdag) {
    std::ofstream fout;
    fout.open(out, std::ios::out | std::ios::binary);
    if (!fout.is_open())
      throw "Could not open output file";

    // Write resolution metadata
    fout.write(reinterpret_cast<char*>(&resolution), sizeof(uint));

    // SparseVoxelDAG svdag(resolution);
    const std::vector<std::vector<std::vector<bool>>>& voxelData = voxelGrid.getVoxelData();

    std::vector<std::array<uint32_t, 8>> indices;
    std::vector<std::tuple<uint, uint, glm::vec3, uint>> queue;
    uint completedCount = 0;
    uint total = (std::log2(resolution)+1) * resolution * resolution * resolution;
    double totalInv = 1.f/total;

    std::future<void> f = startProgressBar(&voxelGrid.mDefaultLogMutex, [&completedCount, &totalInv](){ return completedCount * totalInv; }, "Generating SVDAG");

    generateSVDAGTopDown(indices, voxelData, glm::vec3(0, 0, 0), resolution, queue, completedCount);
    uint queueIndex = 0;

    while (queueIndex < queue.size()) {
      auto queueItem = queue.at(queueIndex);
      indices.at(std::get<0>(queueItem)).at(std::get<1>(queueItem)) = generateSVDAGTopDown(indices, voxelData, std::get<2>(queueItem), std::get<3>(queueItem), queue, completedCount);
      // std::println("total: {}, {}", completedCount, total);
      ++queueIndex;
    }

    {
      std::lock_guard<std::mutex> lock(voxelGrid.mDefaultLogMutex);
      std::println("Writing...");
    }

    fout.write(reinterpret_cast<char*>(&indices.at(0)), indices.size() * 8 * sizeof(uint32_t));

    fout.close();
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

uint generateSVDAGTopDown(std::vector<std::array<uint, 8>>& pIndices, const std::vector<std::vector<std::vector<bool>>>& pVoxelData, glm::vec3 pNodeOrigin, uint pNodeSize, std::vector<std::tuple<uint, uint, glm::vec3, uint>>& pQueue, uint& pCompletedCount) {
  bool allZero = true;
  bool allOne = true;
  uint total = pCompletedCount + pNodeSize * pNodeSize * pNodeSize;
  // {
    // VMesh::ScopedTimer t("Scanning");
    for (uint x = pNodeOrigin.x; x < pNodeOrigin.x + pNodeSize; ++x) {
      for (uint y = pNodeOrigin.y; y < pNodeOrigin.y + pNodeSize; ++y) {
        for (uint z = pNodeOrigin.z; z < pNodeOrigin.z + pNodeSize; ++z) {
          const bool& v = pVoxelData[x][y][z];
          allZero = allZero && v == 0;
          allOne = allOne && v == 1;
          if (!allOne && !allZero)
            break;
          ++pCompletedCount;
        }
      }
    }
  // }
  pCompletedCount = total;

  if (allZero || allOne)
    pCompletedCount += std::log2(pNodeSize) * pNodeSize * pNodeSize * pNodeSize;
  if (allZero)
    return 0;
  else if (allOne)
    return 1;

  pNodeSize = pNodeSize >> 1;

  // {
    // VMesh::ScopedTimer t("Adding to queue");
    pIndices.emplace_back();
    for (uint i = 0; i < 8; ++i)
      pQueue.push_back({pIndices.size()-1, i, pNodeOrigin + (float)pNodeSize * toPos(i), pNodeSize});
  // }

  return pIndices.size() + 1;
}

uint generateSVDAGBottomUp(std::vector<std::array<uint, 8>>& pIndices, const std::vector<std::vector<std::vector<bool>>>& pVoxelData, glm::vec3 pNodeOrigin, uint pNodeSize, std::vector<std::tuple<uint, uint, glm::vec3, uint>>& pQueue, uint& pCompletedCount) {
  bool allZero = true;
  bool allOne = true;
  // uint total = pCompletedCount + pNodeSize * pNodeSize * pNodeSize;
  for (uint x = pNodeOrigin.x; x < pNodeOrigin.x + pNodeSize; ++x) {
    for (uint y = pNodeOrigin.y; y < pNodeOrigin.y + pNodeSize; ++y) {
      for (uint z = pNodeOrigin.z; z < pNodeOrigin.z + pNodeSize; ++z) {
        if (pVoxelData[x][y][z] != 0)
          allZero = false;
        if (pVoxelData[x][y][z] != 1)
          allOne = false;
        if (!allOne && !allZero)
          break;
        // ++pCompletedCount;
      }
    }
  }

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

