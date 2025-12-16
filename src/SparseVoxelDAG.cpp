#include "SparseVoxelDAG.hpp"

SparseVoxelDAG::SparseVoxelDAG(uint pSize)
:mSize(pSize), mMaxDepth(log2(pSize)) {
  mIndices.push_back({0, 0, 0, 0, 0, 0, 0, 0}); // Air node
}

void SparseVoxelDAG::insert(const glm::vec3& pPoint) {
  insertImpl(pPoint, 2, mSize, glm::vec3(0, 0, 0));
}

uint SparseVoxelDAG::getSize() {
  return mSize;
}

void SparseVoxelDAG::print() {
  std::println("\n------------------------------------");
  std::println("Indices:");
  for (auto node: mIndices)
    std::println("{}, {}, {}, {}, {}, {}, {}, {}", node[0], node[1], node[2], node[3], node[4], node[5], node[6], node[7]);
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
  uint& node = mIndices[pNodeIndex - 2][toChildIndex(pos)]; // Minus 2 because index 0 is used for air and 1 for a  voxel
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
    mIndices.push_back({0, 0, 0, 0, 0, 0, 0, 0}); // Air node
    // node reference is now invalid
  }

  insertImpl(pPoint, pNodeIndex, pNodeSize, pNodeOrigin);
}

