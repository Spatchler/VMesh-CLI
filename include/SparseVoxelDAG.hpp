#pragma once

#include "VMesh/timer.hpp"

#include <vector>
#include <print>
#include <algorithm>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class SparseVoxelDAG {
public:
  SparseVoxelDAG(uint pSize);

  void insert(const glm::vec3& pPoint);

  uint getSize();

  void print();

  static uint toChildIndex(glm::vec3 pPos);
  static glm::vec3 toPos(uint pChildIndex);

  std::vector<std::array<uint, 8>> mIndices;
protected:
  void insertImpl(const glm::vec3& pPoint, uint pNodeIndex, uint pNodeSize, glm::vec3 pNodeOrigin);

  uint mSize, mMaxDepth;
};

