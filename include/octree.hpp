#pragma once

#include <print>
#include <vector>
#include <array>
#include <limits>
#include <memory>
#include "VMesh/voxelGrid.hpp"

struct Node {
  Node() = default;
  Node(uint32_t i) : index(i) {};
  uint32_t index = 0;
  std::array<std::shared_ptr<Node>, 8> children;
};

class Palette {
public:
  Palette(uint pSize);

  void resize(uint pSize);

  uint size() { return mNodes.size(); };

  std::vector<std::shared_ptr<Node>> mNodes;
};

class Octree {
public:
  Octree(uint pResolution, Palette* pPalette);
  Octree(VMesh::VoxelGrid& pGrid, Palette* pPalette, uint64_t* pCompletedCount = NULL);

  void attach(Octree& pOctree, const glm::uvec3& pOrigin);

  std::vector<std::array<uint32_t, 8>> generateIndices();

  uint getResolution();

  void write(std::string pPath);

  void generateNode();
  void processNode(std::shared_ptr<Node> pNode, std::vector<std::array<uint32_t*, 8>>& pArangement, std::vector<std::shared_ptr<Node>>& pQueue);
  static uint toChildIndex(const glm::uvec3& pPos);
  static glm::uvec3 toChildPos(uint8_t pIndex);

  uint mResolution;
  std::vector<std::shared_ptr<Node>> mNodes;
  Palette* mPalette; // TODO: Use shared ptr instead of raw ptr
};
