#pragma once

#include <vector>
#include <array>
#include <algorithm>
#include "VMesh/voxelGrid.hpp"

class Node {
public:
  Node(bool pIsLeaf, bool pIsAir, uint pSize, glm::uvec3 pOrigin);

  void createChildren(std::vector<std::array<uint32_t, 8>>& pIndices, std::vector<Node*>& pQueue);
  void evaluateChildrenIndices(std::vector<std::array<uint32_t, 8>>& pIndices);
  void generate(VMesh::VoxelGrid& pGrid, std::vector<Node*>& pQueue, uint64_t& pCompletedCount);

  static uint toChildIndex(glm::uvec3 pPos);
  static glm::uvec3 toPos(uint pChildIndex);
  
  bool isLeaf = false, isAir = false;
  uint32_t index = 0;

  glm::uvec3 origin = glm::uvec3(0);
  uint size = 0;

  static Node* sSolid;
  static Node* sAir;
  std::array<Node*, 8> children = {{NULL}};
};

class SparseVoxelOctree {
public:
  SparseVoxelOctree(uint pSize);
  SparseVoxelOctree(VMesh::VoxelGrid& pGrid, uint64_t* pCompletedCount = NULL);

  void attachSVO(SparseVoxelOctree& pSVO, const glm::uvec3& pOrigin);
  
  std::vector<std::array<uint32_t, 8>> generateIndices();

  uint mSize;
  uint mMaxDepth;

  Node* mRootNode = NULL;
};
