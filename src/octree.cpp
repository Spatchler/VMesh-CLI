#include <octree.hpp>

Octree::Octree(uint pResolution, uint pPaletteSize)
:mResolution(pResolution) {
  // Init palette
  for (uint i = 0; i <= pPaletteSize; ++i)
    mPalette.emplace_back(std::make_shared<Node>(std::numeric_limits<uint32_t>::max() - pPaletteSize + i));
}

Octree::Octree(VMesh::VoxelGrid& pGrid, uint64_t* pCompletedCount)
:mResolution(pGrid.getResolution()) {
  uint64_t completedCount;
  if (!pCompletedCount)
    pCompletedCount = &completedCount;

  // Init palette
  for (uint i = 0; i <= pGrid.mPalette.size(); ++i)
    mPalette.emplace_back(std::make_shared<Node>(std::numeric_limits<uint32_t>::max() - pGrid.mPalette.size() + i));

  if (pGrid.getVoxelCount() == 0 || (pGrid.getVoxelCount() == pGrid.getVolume() && pGrid.isRegionAllSame(glm::uvec3(0), mResolution)) ) {
    mNodes.emplace_back(mPalette.at(pGrid.queryVoxelData(glm::uvec3(0))));
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
      mNodes.at(index >> 3)->children[index & 0b111] = mPalette.at(pGrid.queryVoxelData(origin));
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

void Octree::attach(Octree& pOctree, glm::uvec3& pOrigin) {
  if (pOctree.getResolution() > mResolution) throw std::runtime_error("Can't attach a larger octree");
  glm::uvec3 o(0);
  uint size = mResolution;
  std::shared_ptr<Node>& n = mNodes.size() == 0 ? mNodes.emplace_back(std::make_shared<Node>()) : mNodes.at(0);
  while ((size >>= 1) != (pOctree.getResolution() >> 1)) {
    uint i = toChildIndex((pOrigin - o) / size);
    o = toChildPos(i) * size;
    if (!n->children[i]) n->children[i] = std::make_shared<Node>();
    n = n->children[i];
  }
  n = pOctree.mNodes.at(0);
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

void Octree::resizePalette(uint pSize) {
  mPalette.erase(mPalette.begin() + pSize, mPalette.end());
  for (uint i = 0; i < mPalette.size(); ++i)
    mPalette[i]->index = std::numeric_limits<uint32_t>::max() - mPalette.size() + i;
}

uint Octree::getResolution() {
  return mResolution;
}

void Octree::write(std::string pPath) {
  // Write octree
  pPath.append(".vm8");
  std::println("Generating indices");
  VMesh::Timer t;
  std::vector<std::array<uint32_t, 8>> indices = generateIndices();
  std::println("Generating indices took: {}", t.getTime());

  t.start();
  std::println("Writing octree to: {}", pPath);

  std::ofstream fout;
  fout.open(pPath, std::ios::out | std::ios::binary);
  if (!fout.is_open()) throw std::runtime_error("Could not open output file");

  // Header
  fout << "VMeshOctree\n0100\n";
  // Resolution
  fout.write(reinterpret_cast<char*>(&mResolution), sizeof(uint32_t));
  // Palette size
  uint32_t paletteSize = mPalette.size();
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
    std::shared_ptr<Node> n = pNode->children[i] ? pNode->children[i] : mPalette.at(0);
    pArangement.back()[i] = &n->index;
    if (n->index < std::numeric_limits<uint32_t>::max() - mPalette.size()) // Isnt leaf
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
