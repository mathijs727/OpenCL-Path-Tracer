#include "top_bvh.h"
#include "scene.h"
#include <algorithm>
#include <array>
#include <numeric>
#include <stack>
//#include <crtdbg.h>

using namespace raytracer;

uint32_t raytracer::TopLevelBvhBuilder::build(
    std::vector<SubBvhNode>& subBvhNodes,
    std::vector<TopBvhNode>& outTopNodes)
{
    _sub_bvh_nodes = &subBvhNodes;
    _top_bvh_nodes = &outTopNodes;

    // Add the scene graph nodes to teh top-level BVH buffer
    std::vector<uint32_t> list;
    std::stack<std::pair<SceneNode*, glm::mat4>> nodeStack;
    nodeStack.push(std::make_pair(&_scene.getRootNode(), glm::mat4()));
    while (!nodeStack.empty()) {
        std::pair<SceneNode*, glm::mat4> currentPair = nodeStack.top();
        nodeStack.pop();
        SceneNode* sceneNode = currentPair.first;
        glm::mat4 matrix = sceneNode->transform.matrix();
        glm::mat4 transform = currentPair.second * matrix;

        // Visit children
        for (unsigned i = 0; i < sceneNode->children.size(); i++) {
            nodeStack.push(std::make_pair(sceneNode->children[i].get(), transform));
        }

        if (sceneNode->mesh == -1)
            continue;

        if (_scene.getMeshes()[sceneNode->mesh].mesh > 0) {
            uint32_t nodeId = (uint32_t)outTopNodes.size();
            outTopNodes.push_back(createNode(sceneNode, transform));
            list.push_back(nodeId);
        }
    }

    // Slide 50: http://www.cs.uu.nl/docs/vakken/magr/2016-2017/slides/lecture%2004%20-%20real-time%20ray%20tracing.pdf
    // Fast Agglomerative Clustering for Rendering (Walter et al, 2008)
    uint32_t nodeA = list.back(); // list.pop_back();
    uint32_t nodeB = findBestMatch(list, nodeA);
    while (list.size() > 1) {
        uint32_t nodeC = findBestMatch(list, nodeB);
        if (nodeA == nodeC) {
            // http://stackoverflow.com/questions/39912/how-do-i-remove-an-item-from-a-stl-vector-with-a-certain-value
            auto newEndA = std::remove(list.begin(), list.end(), nodeA);
            list.erase(newEndA);
            auto newEndB = std::remove(list.begin(), list.end(), nodeB);
            list.erase(newEndB);

            // A = new Node(A, B);
            uint32_t nodeId = (uint32_t)_top_bvh_nodes->size();
            _top_bvh_nodes->push_back(mergeNodes(nodeA, nodeB));
            nodeA = nodeId;

            list.push_back(nodeA);
            nodeB = findBestMatch(list, nodeA);
        } else {
            nodeA = nodeB;
            nodeB = nodeC;
        }
    }

    return (uint32_t)outTopNodes.size() - 1; // Root node is at the end
}

uint32_t raytracer::TopLevelBvhBuilder::findBestMatch(const std::vector<uint32_t>& list, uint32_t nodeId)
{
    TopBvhNode& node = (*_top_bvh_nodes)[nodeId];
    float curMinArea = std::numeric_limits<float>::max();
    uint32_t curMin = -1;
    for (uint32_t otherNodeId : list) {
        TopBvhNode& otherNode = (*_top_bvh_nodes)[otherNodeId];
        AABB bounds = calcCombinedBounds(node.bounds, otherNode.bounds);
        glm::vec3 extents = bounds.max - bounds.min;
        float leftArea = extents.z * extents.y;
        float topArea = extents.x * extents.y;
        float backArea = extents.x * extents.z;
        float totalArea = leftArea + topArea + backArea;

        if (totalArea < curMinArea && otherNodeId != nodeId) {
            curMin = otherNodeId;
            curMinArea = totalArea;
        }
    }

    return curMin;
}

TopBvhNode raytracer::TopLevelBvhBuilder::createNode(const SceneNode* sceneGraphNode, const glm::mat4 transform)
{
    auto& bvhMeshPair = _scene.getMeshes()[sceneGraphNode->mesh];

    TopBvhNode node;
    node.subBvhNode = bvhMeshPair.mesh->getBvhRootNode() + bvhMeshPair.bvhOffset;
    node.bounds = calcTransformedAABB((*_sub_bvh_nodes)[node.subBvhNode].bounds, transform);
    node.invTransform = glm::inverse(transform);
    node.isLeaf = true;
    return node;
}

TopBvhNode raytracer::TopLevelBvhBuilder::mergeNodes(uint32_t nodeId1, uint32_t nodeId2)
{
    const TopBvhNode& node1 = (*_top_bvh_nodes)[nodeId1];
    const TopBvhNode& node2 = (*_top_bvh_nodes)[nodeId2];

    TopBvhNode node;
    node.bounds = calcCombinedBounds(node1.bounds, node2.bounds);
    node.invTransform = glm::mat4(); // Identity
    node.leftChildIndex = nodeId1;
    node.rightChildIndex = nodeId2;
    node.isLeaf = false;
    return node;
}

AABB raytracer::TopLevelBvhBuilder::calcCombinedBounds(const AABB& bounds1, const AABB& bounds2)
{
    AABB result;
    result.min = glm::min(bounds1.min, bounds2.min);
    result.max = glm::max(bounds1.max, bounds2.max);
    return result;
}

AABB raytracer::TopLevelBvhBuilder::calcTransformedAABB(const AABB& bounds, glm::mat4 transform)
{
    glm::vec3 min = bounds.min;
    glm::vec3 max = bounds.max;

    glm::vec3 corners[8];
    corners[0] = glm::vec3(transform * glm::vec4(max.x, max.y, max.z, 1.0f));
    corners[1] = glm::vec3(transform * glm::vec4(max.x, max.y, min.z, 1.0f));
    corners[2] = glm::vec3(transform * glm::vec4(max.x, min.y, max.z, 1.0f));
    corners[3] = glm::vec3(transform * glm::vec4(max.x, min.y, min.z, 1.0f));
    corners[4] = glm::vec3(transform * glm::vec4(min.x, min.y, min.z, 1.0f));
    corners[5] = glm::vec3(transform * glm::vec4(min.x, min.y, max.z, 1.0f));
    corners[6] = glm::vec3(transform * glm::vec4(min.x, max.y, min.z, 1.0f));
    corners[7] = glm::vec3(transform * glm::vec4(min.x, max.y, max.z, 1.0f));

    glm::vec3 newMin = corners[0];
    glm::vec3 newMax = corners[0];
    for (int i = 1; i < 8; i++) {
        newMin = glm::min(corners[i], newMin);
        newMax = glm::max(corners[i], newMax);
    }

    AABB result;
    result.min = newMin;
    result.max = newMax;
    return result;
}
