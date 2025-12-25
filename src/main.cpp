#include "VMesh/model.hpp"
#include "VMesh/voxelGrid.hpp"

#include "SparseVoxelDAG.hpp"
#include "progressBar.hpp"

#include <boost/program_options.hpp>
#include <iostream>
#include <fstream>

namespace po = boost::program_options;

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

  if (isSvdag && isCompressed) {
    std::println("There is currently no support for compressed SVDAGs");
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
  SparseVoxelDAG svdag(resolution);

  // Set insert function if generating an svdag
  std::function<void(float,float,float)> insertFunc = [](...){};
  if (isSvdag) {
    std::println("Generating Sparse Voxel DAG of resolution {}", resolution);

    insertFunc = [&svdag](float x, float y, float z) {
      svdag.insert(glm::vec3(x,y,z));
    };
  }
  else
    std::println("Generating Voxel Data of resolution {}", resolution);

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
  std::future<void> f = startProgressBar(&voxelGrid.mDefaultLogMutex, [&trisComplete, &triCountInv](){ return trisComplete * triCountInv; }, "Generating");
  voxelGrid.voxelizeMesh(model, &trisComplete, insertFunc);

  // #########
  // - Write -
  // #########

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

    if (!isCompressed) // Uncompressed svdag
      fout.write(reinterpret_cast<char*>(&svdag.mIndices.at(0)), svdag.mIndices.size() * (8 * sizeof(uint)));
    else
      return 1;

    fout.close();
  }


  return 0;
}

