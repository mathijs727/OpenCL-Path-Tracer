#pragma once
#include "imesh.h"
#include "material.h"
#include "opencl/texture.h"
#include "transform.h"
#include "vertices.h"
#include <filesystem>
#include <glm/glm.hpp>
#include <optional>
#include <vector>

struct aiMesh;
struct aiScene;

namespace raytracer {

class Mesh : public IMesh {
public:
    Mesh(const std::filesystem::path& filePath, const Transform& offset, const Material& overrideMaterial, UniqueTextureArray& textureArray);
    Mesh(const std::filesystem::path& filePath, const Transform& offset, UniqueTextureArray& textureArray);
    Mesh(const std::filesystem::path& filePath, const Material& overrideMaterial, UniqueTextureArray& textureArray);
    Mesh(const std::filesystem::path& filePath, UniqueTextureArray& textureArray);
    ~Mesh() = default;

    std::span<const VertexSceneData> getVertices() const override { return m_vertices; }
    std::span<const TriangleSceneData> getTriangles() const override { return m_triangles; }
    std::span<const Material> getMaterials() const override { return m_materials; }
    std::span<const SubBVHNode> getBvhNodes() const override { return m_bvhNodes; }
    std::span<const uint32_t> getEmissiveTriangles() const override { return m_emissiveTriangles; }

    uint32_t getBvhRootNode() const override { return m_bvhRootNode; };

    AABB getBounds() const override { return m_bounds; }
    bool isDynamic() const override { return false; };
    uint32_t maxNumVertices() const override { return (uint32_t)m_vertices.size(); };
    uint32_t maxNumTriangles() const override { return (uint32_t)m_triangles.size(); };
    uint32_t maxNumMaterials() const override { return (uint32_t)m_materials.size(); };
    uint32_t maxNumBvhNodes() const override { return (uint32_t)m_bvhNodes.size(); };
    void buildBvh() override {}; // Only necessary for dynamic objects
private:
    void loadFromFile(
        const std::filesystem::path& filePath,
        const Transform& offset,
        std::optional<Material> overrideMaterial,
        UniqueTextureArray& textureArray);

    void addSubMesh(
        const aiScene* scene,
        unsigned meshIndex,
        const glm::mat4& transformMatrix,
        const std::filesystem::path& textureBasePath,
        UniqueTextureArray& textureArray,
        std::optional<Material> overrideMaterial);
    void collectEmissiveTriangles();

    void storeBvh(const char* fileName);
    bool loadBvh(const char* fileName);

private:
    std::vector<VertexSceneData> m_vertices;
    std::vector<TriangleSceneData> m_triangles;
    std::vector<uint32_t> m_emissiveTriangles;
    std::vector<Material> m_materials;
    std::vector<SubBVHNode> m_bvhNodes;

    AABB m_bounds;
    uint32_t m_bvhRootNode;
};
}
