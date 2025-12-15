#pragma once

#include "VMesh/timer.hpp"

#include <vector>
#include <print>
#include <algorithm>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

struct VoxelData {
  glm::vec4 color;
};

constexpr bool operator==(const VoxelData& lhs, const VoxelData& rhs) {
  return lhs.color == rhs.color;
}

class SparseVoxelDAG {
public:
  SparseVoxelDAG(uint pSize);

  void insert(const glm::vec3& pPoint, const VoxelData& pData);

  uint getSize();
  uint getMidpoint();

  void print();

  static uint toChildIndex(glm::vec3 pPos);
  static glm::vec3 toPos(uint pChildIndex);

  std::vector<std::array<uint, 8>> mIndices;
  std::vector<VoxelData> mData;
protected:
  void insertImpl(const glm::vec3& pPoint, const uint& pData, uint pNodeIndex, uint pNodeSize, glm::vec3 pNodeOrigin);

  uint mSize, mMaxDepth, mMidpoint;
};

