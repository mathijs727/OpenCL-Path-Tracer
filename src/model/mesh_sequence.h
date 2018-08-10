#pragma once
#include "imesh.h"
#include "material.h"
#include "opencl/texture.h"
#include "transform.h"
#include "vertices.h"
#include <glm/glm.hpp>
#include <optional>
#include <string_view>
#include <vector>

struct aiMesh;
struct aiScene;

namespace raytracer {
class MeshSequence : public IMesh {
public:
    MeshSequence(std::string_view fileName, bool refitting, UniqueTextureArray& textureArray);
    MeshSequence(std::string_view fileName, const Transform& transform, bool refitting, UniqueTextureArray& textureArray);
    MeshSequence(gsl::span<std::string_view> files, bool refitting, UniqueTextureArray& textureArray);
    MeshSequence(gsl::span<std::string_view> files, const Transform& transform, bool refitting, UniqueTextureArray& textureArray);
    ~MeshSequence() = default;

    void goToNextFrame();

    gsl::span<const VertexSceneData> getVertices() const override { return m_frames[m_currentFrame].vertices; }
    gsl::span<const TriangleSceneData> getTriangles() const override { return m_frames[m_currentFrame].triangles; }
    gsl::span<const Material> getMaterials() const override { return m_frames[m_currentFrame].materials; }
    gsl::span<const SubBVHNode> getBvhNodes() const override { return m_bvhNodes; }

    gsl::span<const uint32_t> getEmissiveTriangles() const override { return {}; } // Not implemented yet

    uint32_t getBvhRootNode() const override { return m_bvhRootNode; };

    bool isDynamic() const override { return true; };
    uint32_t maxNumVertices() const override;
    uint32_t maxNumTriangles() const override;
    uint32_t maxNumMaterials() const override;
    uint32_t maxNumBvhNodes() const override;
    void buildBvh() override;

private:
    void addSubMesh(
        const aiScene& scene,
        unsigned meshIndex,
        const glm::mat4& transformMatrix,
        std::string_view texturePath,
        UniqueTextureArray& textureArray,
        std::vector<VertexSceneData>& outVertices,
        std::vector<TriangleSceneData>& outTriangles,
        std::vector<Material>& outMaterials);
    void loadFile(std::string_view fileName, const Transform& offset, UniqueTextureArray& textureArray);
    void loadFiles(gsl::span<std::string_view> files, const Transform& offset, UniqueTextureArray& textureArray);

private:
    struct MeshFrame {
        std::vector<VertexSceneData> vertices;
        std::vector<TriangleSceneData> triangles;
        std::vector<Material> materials;
    };

    bool m_bvhNeedsUpdate = false;

    bool m_refitting;
    std::vector<SubBVHNode> m_bvhNodes;
    uint32_t m_bvhRootNode;

    unsigned m_currentFrame = 0;
    std::vector<MeshFrame> m_frames;
};
}
