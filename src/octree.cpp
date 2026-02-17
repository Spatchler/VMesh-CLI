#include "octree.hpp"

Node* Node::sSolid = new Node(true, false, 0, glm::uvec3(0));
Node* Node::sAir = new Node(true, true, 0, glm::uvec3(0));

Node::Node(bool pIsLeaf, bool pIsAir, uint pSize, glm::uvec3 pOrigin)
:isLeaf(pIsLeaf), isAir(pIsAir), size(pSize), origin(pOrigin) {
  // std::println("Constructing node with origin: {}, {}, {}", origin.x, origin.y, origin.z);
  children = {sAir, sAir, sAir, sAir, sAir, sAir, sAir, sAir};
}

void Node::createChildren(std::vector<std::array<uint32_t, 8>>& pIndices, std::vector<Node*>& pQueue) {
  for (Node* i: children) if (!i->isLeaf) {
    i->index = pIndices.size();
    pQueue.push_back(i);
    pIndices.emplace_back();
  }
}

void Node::evaluateChildrenIndices(std::vector<std::array<uint32_t, 8>>& pIndices) {
  for (uint i = 0; i < 8; ++i) {
    if (!children[i]->isLeaf)    pIndices[index][i] = children[i]->index + 2;
    else if (children[i]->isAir) pIndices[index][i] = 0;
    else                         pIndices[index][i] = 1;
  }
}

void Node::generate(VMesh::VoxelGrid& pGrid, std::vector<Node*>& pQueue, uint64_t& pCompletedCount) {
  uint childSize = size >> 1;
  uint64_t childVolume = childSize;
  childVolume = childVolume * childVolume * childVolume;

  if (childSize == 0) return;

  for (uint i = 0; i < 8; ++i) {
    uint64_t total = pCompletedCount + childVolume;

    glm::uvec3 o = origin + toPos(i) * childSize;

    bool allZero = true;
    bool allOne = true;

    for (uint z = o.z; z < o.z + childSize; ++z)
      for (uint y = o.y; y < o.y + childSize; ++y)
        for (uint x = o.x; x < o.x + childSize && (allOne || allZero); ++x,++pCompletedCount) {
          const bool v = pGrid.queryVoxel(glm::vec3(x, y, z));
          allZero = allZero && v == 0;
          allOne = allOne && v == 1;
        }

    pCompletedCount = total;

    if (allZero) children[i] = sAir;
    else if (allOne) children[i] = sSolid;

    if (allZero || allOne) {
      pCompletedCount += std::log2(childSize) * childVolume;
      continue;
    }

    children[i] = new Node(false, false, childSize, o);
    pQueue.push_back(children[i]);
  }
}

uint Node::toChildIndex(glm::uvec3 pPos) {
  glm::tvec3<int, glm::packed_highp> localChildPos = {
    int(std::min(1.0, floor(pPos.x))),
    int(std::min(1.0, floor(pPos.y))),
    int(std::min(1.0, floor(pPos.z)))
  };
  return (localChildPos.x << 0) | (localChildPos.y << 1) | (localChildPos.z << 2); // Index in childIndices 0 - 7
}

glm::uvec3 Node::toPos(uint pChildIndex) {
  glm::uvec3 pos;
  pos.x = 1 & pChildIndex;
  pos.y = ((1 << 1) & pChildIndex) != 0;
  pos.z = ((1 << 2) & pChildIndex) != 0;
  return pos;
}

SparseVoxelOctree::SparseVoxelOctree(uint pSize)
:mSize(pSize), mMaxDepth(std::log2f(pSize)) {
  mRootNode = new Node(false, false, mSize, glm::uvec3(0));
}

SparseVoxelOctree::SparseVoxelOctree(VMesh::VoxelGrid& pGrid, uint64_t* pCompletedCount)
:mSize(pGrid.getResolution()), mMaxDepth(pGrid.getMaxDepth()) {
  mRootNode = new Node(false, false, mSize, glm::uvec3(0));

  uint64_t completedCount = 0;
  if (!pCompletedCount) pCompletedCount = &completedCount;

  std::vector<Node*> queue;
  mRootNode->generate(pGrid, queue, *pCompletedCount);
  for (uint i = 0; i < queue.size(); ++i)
    queue[i]->generate(pGrid, queue, *pCompletedCount);
}

void SparseVoxelOctree::attachSVO(SparseVoxelOctree& pSVO, const glm::uvec3& pOrigin) {
  if (pSVO.mSize == mSize) {
    mRootNode = pSVO.mRootNode;
    return;
  }
  Node** node = &mRootNode;
  for (uint nodeSize = mSize >> 1; nodeSize >= pSVO.mSize; nodeSize >>= 1) {
    glm::uvec3 o = pOrigin - (*node)->origin;
    node = &(*node)->children[Node::toChildIndex(o / nodeSize)];
    if ((*node)->isLeaf || !*node)
      *node = new Node(false, false, nodeSize, (*node)->origin + o / nodeSize * nodeSize);
    (*node)->isLeaf = false;
  }
  *node = pSVO.mRootNode;
}

std::vector<std::array<uint32_t, 8>> SparseVoxelOctree::generateIndices() {
  // Create nodes
  std::vector<std::array<uint32_t, 8>> indices;
  std::vector<Node*> queue;
  indices.emplace_back();
  mRootNode->index = 0;
  mRootNode->createChildren(indices, queue);
  for (uint i = 0; i < queue.size(); ++i)
    queue[i]->createChildren(indices, queue);
  // Evaluate indices
  mRootNode->evaluateChildrenIndices(indices);
  for (uint i = 0; i < queue.size(); ++i)
    queue[i]->evaluateChildrenIndices(indices);
  return indices;
}
