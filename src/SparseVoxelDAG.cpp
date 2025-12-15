#include "SparseVoxelDAG.hpp"

SparseVoxelDAG::SparseVoxelDAG(uint pSize)
:mSize(pSize), mMaxDepth(log2(pSize)), mMidpoint(UINT_MAX / 2) {
  mIndices.push_back({mMidpoint, mMidpoint, mMidpoint, mMidpoint, mMidpoint, mMidpoint, mMidpoint, mMidpoint});
}

void SparseVoxelDAG::insert(const glm::vec3& pPoint, const VoxelData& pData) {
  const auto result = std::find(std::begin(mData), std::end(mData), pData);
  uint index;
  if (result != mData.end())
    index = std::distance(mData.begin(), result);
  else {
    index = mData.size();
    mData.push_back(pData);
  }
  insertImpl(pPoint, index, 0, mSize, glm::vec3(0, 0, 0));
}

uint SparseVoxelDAG::getSize() {
  return mSize;
}

uint SparseVoxelDAG::getMidpoint() {
  return mMidpoint;
}

void SparseVoxelDAG::print() {
  std::println("\n------------------------------------");
  std::println("Indices:");
  for (auto node: mIndices) {
    std::println("{}, {}, {}, {}, {}, {}, {}, {}", node[0], node[1], node[2], node[3], node[4], node[5], node[6], node[7]);
  }
  std::println("Data:");
  for (VoxelData& node: mData) {
    std::println("{}, {}, {}, {}", node.color.x, node.color.y, node.color.z, node.color.w);
  }
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

void SparseVoxelDAG::insertImpl(const glm::vec3& pPoint, const uint& pDataIndex, uint pNodeIndex, uint pNodeSize, glm::vec3 pNodeOrigin) {
  // TODO: Add dynamic midpoint
  pNodeSize = pNodeSize >> 1;

  glm::vec3 pos = pPoint;
  pos -= pNodeOrigin;
  pos /= pNodeSize;
  pos = glm::floor(pos);
  pNodeOrigin += pos * (float)pNodeSize;
  uint& node = mIndices[pNodeIndex][toChildIndex(pos)];
  pNodeIndex = node;

  if (pNodeSize == 1) { // Insert data index once we reach max depth
    node = mMidpoint + pDataIndex;
    return;
  }

  // If node doesn't exist create it
  if (node == 0 || node == mMidpoint) {
    pNodeIndex = mIndices.size();
    node = mIndices.size();
    mIndices.push_back({mMidpoint, mMidpoint, mMidpoint, mMidpoint, mMidpoint, mMidpoint, mMidpoint, mMidpoint}); // Air node
  }

  insertImpl(pPoint, pDataIndex, pNodeIndex, pNodeSize, pNodeOrigin);
}

