#pragma once

#include "VMesh/timer.hpp"

#include <vector>
#include <print>
#include <algorithm>
#include <unordered_map>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class SparseVoxelDAG {
public:
  SparseVoxelDAG(uint pSize);

  void insert(const glm::vec3& pPoint);

  void compress();

  uint getSize();

  void print();

  static uint toChildIndex(glm::vec3 pPos);
  static glm::vec3 toPos(uint pChildIndex);

  // std::vector<std::array<uint, 8>> mIndices;
  std::unordered_map<uint, std::array<uint, 8>> mIndices;
protected:
  void insertImpl(const glm::vec3& pPoint, uint pNodeIndex, uint pNodeSize, glm::vec3 pNodeOrigin);
  uint compressImpl(uint pNodeIndex);
  void deleteChildrenImpl(uint pNodeIndex);

  uint mSize, mMaxDepth, mGreatestIndex = 0;
};

