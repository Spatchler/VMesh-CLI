#include <octree.hpp>

Palette::Palette(uint pSize) {
  for (uint i = 0; i <= pSize; ++i)
    mNodes.emplace_back(std::make_shared<Node>(std::numeric_limits<uint32_t>::max() - pSize + i));
}

void Palette::resize(uint pSize) {
  // mPalette.erase(mPalette.begin() + pSize + 1, mPalette.end());
  mNodes.resize(pSize + 1);
  for (uint i = 0; i <= pSize; ++i) 
    mNodes[i]->index = std::numeric_limits<uint32_t>::max() - pSize + i + 1;
}

Octree::Octree(uint pResolution, Palette* pPalette)
:mResolution(pResolution), mPalette(pPalette) {
}

Octree::Octree(VMesh::VoxelGrid& pGrid, Palette* pPalette, uint64_t* pCompletedCount)
:mPalette(pPalette) {
  for (mResolution = 1; mResolution < pGrid.getResolution(); mResolution <<= 1) {}

  uint64_t completedCount;
  if (!pCompletedCount)
    pCompletedCount = &completedCount;

  // Init palette
  // mPalette->resize(pGrid.mPalette.size());

  if (pGrid.getVoxelCount() == 0 || (pGrid.getVoxelCount() == pGrid.getVolume() && pGrid.isRegionAllSame(glm::uvec3(0), mResolution)) ) {
    mNodes.emplace_back(mPalette->mNodes.at(pGrid.queryVoxelData(glm::uvec3(0))));
    *pCompletedCount += std::log2(mResolution) * pGrid.getVolume();
    return;
  }

  *pCompletedCount += pGrid.getVolume();
  
  std::vector<std::tuple<uint, glm::uvec3, uint>> queue;

  mNodes.emplace_back(std::make_shared<Node>());
  for (uint8_t i = 0; i < 8; ++i) queue.push_back({i, toChildPos(i) * (mResolution >> 1), mResolution >> 1});

  for (uint i = 0; i < queue.size(); ++i) {
    uint               index = std::get<0>(queue.at(i));
    const glm::uvec3  origin = std::get<1>(queue.at(i));
    uint                size = std::get<2>(queue.at(i));

    if (pGrid.isRegionAllSame(origin, size)) {
      mNodes.at(index >> 3)->children[index & 0b111] = mPalette->mNodes.at(pGrid.queryVoxelData(origin));
      *pCompletedCount += std::log2(size) * size * size * size;
      continue;
    }

    size >>= 1;
    for (uint j = 0; j < 8; ++j)
      queue.push_back(std::make_tuple(mNodes.size() << 3 | j, origin + toChildPos(j) * size, size));
    std::shared_ptr<Node> n = std::make_shared<Node>();
    mNodes.emplace_back(n);
    mNodes.at(index >> 3)->children[index & 0b111] = n;
    *pCompletedCount += size * size * size << 3;
  }

// Node* generateImpl(const glm::uvec3& pOrigin, uint pSize) {
//   if (pGrid.isRegionAllSame(pOrigin, pSize))
//     return &mPalette.at(pGrid.queryVoxelData(pOrigin));

//   pSize >>= 1;
//   Node* n = new Node;
//   mNodes.emplace_back(n);
//   for (uint i = 0; i < 8; ++i)
//     n->children[i] = generateImpl(pOrigin + toChildPos(i) * pSize, pSize);
//   return n;
// }
}

void Octree::attach(Octree& pOctree, const glm::uvec3& pOrigin) {
  if (pOctree.getResolution() > mResolution) throw std::runtime_error("Can't attach a larger octree");
  glm::uvec3 o(0);
  uint size = mResolution;
  if (mNodes.size() == 0) mNodes.emplace_back(std::make_shared<Node>());
  std::shared_ptr<Node>* n = &mNodes.at(0);
  while ((size >>= 1) != (pOctree.getResolution() >> 1)) {
    uint i = toChildIndex((pOrigin - o) / size);
    o = toChildPos(i) * size;
    if (!(*n)->children[i]) (*n)->children[i] = std::make_shared<Node>();
    n = &(*n)->children[i];
  }
  *n = pOctree.mNodes.at(0);
}

std::vector<std::array<uint32_t, 8>> Octree::generateIndices() {
  std::vector<std::array<uint32_t, 8>> indices;
  std::vector<std::array<uint32_t*, 8>> arangement;
  std::vector<std::shared_ptr<Node>> queue;

  processNode(mNodes.at(0), arangement, queue); // Process root node
  
  for (uint i = 0; i < queue.size(); ++i)
    processNode(queue[i], arangement, queue);

  indices.resize(arangement.size());
  for (uint i = 0; i < arangement.size(); ++i)
    for (uint j = 0; j < 8; ++j)
      indices[i][j] = *arangement[i][j];

  return indices;
}

uint Octree::getResolution() {
  return mResolution;
}

void Octree::write(std::string pPath) {
  // Write octree
  pPath.append(".vm8");
  std::println("Generating indices");
  VMesh::Timer t;
  std::vector<std::array<uint32_t, 8>> indices{{mPalette->mNodes.at(0)->index}};
  if (mNodes.size()) indices = generateIndices();
  std::println("Generating indices took: {}", t.getTime());

  t.start();
  std::println("Writing octree to: \e[1;3;4;33m{}\e[0m", pPath);

  std::ofstream fout;
  fout.open(pPath, std::ios::out | std::ios::binary);
  if (!fout.is_open()) throw std::runtime_error("Could not open output file");

  // Header
  fout << "VMESH8";
  const uint32_t fileVersion = 100;
  fout.write(reinterpret_cast<const char*>(&fileVersion), sizeof(fileVersion));
  // Resolution
  fout.write(reinterpret_cast<char*>(&mResolution), sizeof(uint32_t));
  // Palette size
  uint32_t paletteSize = mPalette->size()  - 1;
  fout.write(reinterpret_cast<char*>(&paletteSize), sizeof(uint32_t));
  // Indices
  uint32_t indicesSize = indices.size();
  fout.write(reinterpret_cast<char*>(&indicesSize), sizeof(uint32_t));
  fout.write(reinterpret_cast<char*>(indices.data()), indices.size() * 8ull * sizeof(uint32_t));

  fout.close();
  std::println("Writing took: {}", t.getTime());
}

void Octree::processNode(std::shared_ptr<Node> pNode, std::vector<std::array<uint32_t*, 8>>& pArangement, std::vector<std::shared_ptr<Node>>& pQueue) {
  pNode->index = pArangement.size();
  pArangement.emplace_back();
  for (uint i = 0; i < 8; ++i) {
    std::shared_ptr<Node> n = pNode->children[i] ? pNode->children[i] : mPalette->mNodes.at(0);
    pArangement.back()[i] = &n->index;
    if (n->index < std::numeric_limits<uint32_t>::max() - mPalette->size()) // Isnt leaf
      pQueue.push_back(n);
  }
}

uint Octree::toChildIndex(const glm::uvec3& pPos) {
  glm::tvec3<int, glm::packed_highp> localChildPos = {
    int(std::min(1.0, floor(pPos.x))),
    int(std::min(1.0, floor(pPos.y))),
    int(std::min(1.0, floor(pPos.z)))
  };
  return (localChildPos.x << 0) | (localChildPos.y << 1) | (localChildPos.z << 2); // Index in childIndices 0 - 7
}

glm::uvec3 Octree::toChildPos(uint8_t pChildIndex) {
  glm::uvec3 pos;
  pos.x = 1 & pChildIndex;
  pos.y = ((1 << 1) & pChildIndex) != 0;
  pos.z = ((1 << 2) & pChildIndex) != 0;
  return pos;
}
