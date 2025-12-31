#include "SparseVoxelDAG.hpp"

SparseVoxelDAG::SparseVoxelDAG(uint pSize)
:mSize(pSize), mMaxDepth(log2(pSize)) {
  // mIndices.push_back({0, 0, 0, 0, 0, 0, 0, 0}); // Air node
  mIndices.insert({0, {0, 0, 0, 0, 0, 0, 0, 0}}); // Air node
  ++mGreatestIndex;
}

void SparseVoxelDAG::insert(const glm::vec3& pPoint) {
  insertImpl(pPoint, 2, mSize, glm::vec3(0, 0, 0));
}

void SparseVoxelDAG::compress() {
  compressImpl(0);
}

uint SparseVoxelDAG::getSize() {
  return mSize;
}

void SparseVoxelDAG::print() {
  std::println("\n------------------------------------");
  std::println("Indices:");
  for (auto i: mIndices)
    std::println("{}, {}, {}, {}, {}, {}, {}, {}", i.second[0], i.second[1], i.second[2], i.second[3], i.second[4], i.second[5], i.second[6], i.second[7]);
  std::println("------------------------------------\n");
}

uint SparseVoxelDAG::toChildIndex(glm::vec3 pPos) {
  glm::tvec3<int, glm::packed_highp> localChildPos = {
    int(floor(std::min(1.f, pPos.x))),
    int(floor(std::min(1.f, pPos.y))),
    int(floor(std::min(1.f, pPos.z)))
  };
  return (localChildPos.x << 0) | (localChildPos.y << 1) | (localChildPos.z << 2); // Index in childIndices 0 - 7
}

glm::vec3 SparseVoxelDAG::toPos(uint pChildIndex) {
  glm::vec3 pos;
  pos.x = 1 & pChildIndex;
  pos.y = ((1 << 1) & pChildIndex) != 0;
  pos.z = ((1 << 2) & pChildIndex) != 0;
  return pos;
}

void SparseVoxelDAG::insertImpl(const glm::vec3& pPoint, uint pNodeIndex, uint pNodeSize, glm::vec3 pNodeOrigin) {
  pNodeSize = pNodeSize >> 1;

  glm::vec3 pos = pPoint;
  pos -= pNodeOrigin;
  pos /= pNodeSize; // TODO: Optimize this division with bit shifts
  pos = glm::floor(pos);
  pNodeOrigin += pos * (float)pNodeSize;
  uint& node = mIndices.at(pNodeIndex - 2)[toChildIndex(pos)]; // Minus 2 because index 0 is used for air and 1 for a  voxel
  pNodeIndex = node;

  if (node == 1) // If we reach a full node, return
    return;

  if (pNodeSize == 1) { // Insert once we reach max depth
    node = 1;
    return;
  }

  // If node doesn't exist create it
  if (node == 0) {
    node = mIndices.size() + 2;
    pNodeIndex = mIndices.size() + 2;
    // mIndices.push_back({0, 0, 0, 0, 0, 0, 0, 0}); // Air node
    mIndices.insert({mGreatestIndex, {0, 0, 0, 0, 0, 0, 0, 0}}); // Air node
    ++mGreatestIndex;
    // node reference is now invalid
  }

  insertImpl(pPoint, pNodeIndex, pNodeSize, pNodeOrigin);
}

uint SparseVoxelDAG::compressImpl(uint pNodeIndex) {
  for (uint i = 0; i < 8; ++i) {
    std::println("began");
    uint index = mIndices.at(pNodeIndex).at(i);
    std::println("finished");
    if (index <= 1)
      continue;
    std::println("running compress");
    uint v = compressImpl(index);
    std::println("finished compress");
    if (v != 3) {
      std::println("Deleting");
      deleteChildrenImpl(index);
      std::println("Passed");
      mIndices.at(pNodeIndex).at(i) = v;
    }
  }

  bool allZero = true;
  bool allOne = true;
  for (uint i: mIndices.at(pNodeIndex)) {
    if (i != 0)
      allZero = false;
    else if (i != 1)
      allOne = false;
  }
  if (allZero)
    return 0;
  if (allOne)
    return 1;
  return 3;

  // ---------------------------

  // if (allZero || allOne)
  //   deleteChildrenImpl(pNodeIndex);
  // if (allZero)
  //   mIndices.at(pNodeIndex) = 0;
  // else if (allOne)
  //   mIndices.at(pNodeIndex) = 1;
  // if (allZero || allOne)
  //   return;

  // for (uint i: mIndices.at(pNodeIndex)) {
  //   if (i > 1)
  //     compressImpl(i);
  // }
}

void SparseVoxelDAG::deleteChildrenImpl(uint pNodeIndex) {
  if (pNodeIndex < 1)
    return;
  pNodeIndex -= 2;
  for (uint i: mIndices.at(pNodeIndex)) {
    if (i > 1)
      deleteChildrenImpl(i);
  }
  mIndices.erase(pNodeIndex);

  // Update indices
  // for (std::array<uint, 8>& a: mIndices) {
    // for (uint& i: a) {
      // if (i > pNodeIndex + 2)
        // i -= 1;
    // }
  // }

  // return pIndexChange;
}

