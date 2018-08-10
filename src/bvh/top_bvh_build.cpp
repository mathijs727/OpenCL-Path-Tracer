#include "top_bvh_build.h"
#include "scene.h"
#include <stack>

namespace raytracer {

static uint32_t findBestMatch(gsl::span<const TopBVHNode> allNodes, gsl::span<uint32_t> indicesToConsider, uint32_t thisNodeID);
static TopBVHNode createNode(const SceneNode& sceneNode, const glm::mat4& transform, gsl::span<const uint32_t> meshBvhOffsets);
static TopBVHNode mergeNodes(uint32_t aID, const TopBVHNode nodeA, uint32_t bID, const TopBVHNode nodeB);
static AABB calcTransformedAABB(const AABB& bounds, glm::mat4 transform);

BvhBuildReturnType buildTopBVH(const SceneNode& rootSceneNode, gsl::span<const uint32_t> meshBvhOffsets)
{
    std::vector<TopBVHNode> outBvhNodes;

    // Add the scene graph nodes to the top-level BVH buffer
    std::vector<uint32_t> list;
    std::stack<std::pair<const SceneNode&, glm::mat4>> nodeStack;
    nodeStack.push(std::make_pair(std::ref(rootSceneNode), glm::mat4(1.0f)));
    while (!nodeStack.empty()) {
        auto [sceneNode, baseTransformMatrix] = nodeStack.top();
        nodeStack.pop();
        glm::mat4 transformMatrix = baseTransformMatrix * sceneNode.transform.matrix();

        for (const auto& child : sceneNode.children)
            nodeStack.push(std::make_pair(std::ref(*child), transformMatrix));

        // Skip internal nodes (no mesh attached) of the scene graph
        if (sceneNode.meshID) {
            uint32_t nodeId = (uint32_t)outBvhNodes.size();
            outBvhNodes.push_back(createNode(sceneNode, transformMatrix, meshBvhOffsets));
            list.push_back(nodeId);
        }
    }

    // Slide 50: http://www.cs.uu.nl/docs/vakken/magr/2016-2017/slides/lecture%2004%20-%20real-time%20ray%20tracing.pdf
    // Fast Agglomerative Clustering for Rendering (Walter et al, 2008)
    uint32_t nodeA = list.back(); // list.pop_back();
    uint32_t nodeB = findBestMatch(outBvhNodes, list, nodeA);
    while (list.size() > 1) {
        uint32_t nodeC = findBestMatch(outBvhNodes, list, nodeB);
        if (nodeA == nodeC) {
            // http://stackoverflow.com/questions/39912/how-do-i-remove-an-item-from-a-stl-vector-with-a-certain-value
            auto newEndA = std::remove(list.begin(), list.end(), nodeA);
            list.erase(newEndA);
            auto newEndB = std::remove(list.begin(), list.end(), nodeB);
            list.erase(newEndB);

            // A = new Node(A, B);
            uint32_t nodeId = (uint32_t)outBvhNodes.size();
            outBvhNodes.push_back(mergeNodes(nodeA, outBvhNodes[nodeA], nodeB, outBvhNodes[nodeB]));
            nodeA = nodeId;

            list.push_back(nodeA);
            nodeB = findBestMatch(outBvhNodes, list, nodeA);
        } else {
            nodeA = nodeB;
            nodeB = nodeC;
        }
    }

    uint32_t rootNode = (uint32_t)outBvhNodes.size() - 1;
    return { rootNode, outBvhNodes }; // Root node is at the end
}

static float calcCombinedSurfaceArea(const TopBVHNode& a, const TopBVHNode& b)
{
    AABB bounds = a.bounds + b.bounds;
    glm::vec3 extents = bounds.max - bounds.min;
    float leftArea = extents.z * extents.y;
    float topArea = extents.x * extents.y;
    float backArea = extents.x * extents.z;
    return 2.0f * (leftArea + topArea + backArea);
}

static uint32_t findBestMatch(gsl::span<const TopBVHNode> allNodes, gsl::span<uint32_t> indicesToConsider, uint32_t thisNodeID)
{
    auto iter = std::min_element(indicesToConsider.begin(), indicesToConsider.end(), [&](uint32_t a, uint32_t b) -> bool {
        if (a == thisNodeID)
            return false;
        if (b == thisNodeID)
            return true;

        float surfaceAreaA = calcCombinedSurfaceArea(allNodes[thisNodeID], allNodes[a]);
        float surfaceAreaB = calcCombinedSurfaceArea(allNodes[thisNodeID], allNodes[b]);
        return surfaceAreaA < surfaceAreaB;
    });
    return *iter;
}

static TopBVHNode createNode(const SceneNode& sceneNode, const glm::mat4& transform, gsl::span<const uint32_t> meshBvhOffsets)
{
    assert(sceneNode.subBvhRootID);
    assert(sceneNode.meshID);

    TopBVHNode node;
    node.subBvhNode = *sceneNode.subBvhRootID + meshBvhOffsets[*sceneNode.meshID];
    node.bounds = calcTransformedAABB(sceneNode.bounds, transform);
    node.invTransform = glm::inverse(transform);
    node.isLeaf = true;
    return node;
}

static TopBVHNode mergeNodes(uint32_t nodeAIndex, const TopBVHNode nodeA, uint32_t nodeBIndex, const TopBVHNode nodeB)
{
    TopBVHNode node;
    node.bounds = nodeA.bounds + nodeB.bounds;
    node.invTransform = glm::mat4(1.0f); // Identity
    node.leftChildIndex = nodeAIndex;
    node.rightChildIndex = nodeBIndex;
    node.isLeaf = false;
    return node;
}

static AABB calcTransformedAABB(const AABB& bounds, glm::mat4 transform)
{
    glm::vec3 min = bounds.min;
    glm::vec3 max = bounds.max;

    std::array<glm::vec3, 8> corners;
    corners[0] = glm::vec3(transform * glm::vec4(max.x, max.y, max.z, 1.0f));
    corners[1] = glm::vec3(transform * glm::vec4(max.x, max.y, min.z, 1.0f));
    corners[2] = glm::vec3(transform * glm::vec4(max.x, min.y, max.z, 1.0f));
    corners[3] = glm::vec3(transform * glm::vec4(max.x, min.y, min.z, 1.0f));
    corners[4] = glm::vec3(transform * glm::vec4(min.x, min.y, min.z, 1.0f));
    corners[5] = glm::vec3(transform * glm::vec4(min.x, min.y, max.z, 1.0f));
    corners[6] = glm::vec3(transform * glm::vec4(min.x, max.y, min.z, 1.0f));
    corners[7] = glm::vec3(transform * glm::vec4(min.x, max.y, max.z, 1.0f));

    AABB result;
    for (auto vertex : corners)
        result.fit(vertex);
    return result;
}

}
