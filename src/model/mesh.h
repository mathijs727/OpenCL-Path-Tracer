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

class Mesh : public IMesh {
public:
    Mesh() = delete;
    ~Mesh() = default;
    Mesh(std::string_view fileName, const Transform& offset, const Material& overrideMaterial, UniqueTextureArray& textureArray);
    Mesh(std::string_view fileName, const Transform& offset, UniqueTextureArray& textureArray);
    Mesh(std::string_view fileName, const Material& overrideMaterial, UniqueTextureArray& textureArray);
    Mesh(std::string_view fileName, UniqueTextureArray& textureArray);

    gsl::span<const VertexSceneData> getVertices() const override { return m_vertices; }
    gsl::span<const TriangleSceneData> getTriangles() const override { return m_triangles; }
    gsl::span<const Material> getMaterials() const override { return m_materials; }
    gsl::span<const SubBVHNode> getBvhNodes() const override { return m_bvhNodes; }
    gsl::span<const uint32_t> getEmissiveTriangles() const override { return m_emissiveTriangles; }

    uint32_t getBvhRootNode() const override { return m_bvhRootNode; };

    AABB getBounds() const override { return m_bounds; }
    bool isDynamic() const override { return false; };
    uint32_t maxNumVertices() const override { return (uint32_t)m_vertices.size(); };
    uint32_t maxNumTriangles() const override { return (uint32_t)m_triangles.size(); };
    uint32_t maxNumMaterials() const override { return (uint32_t)m_materials.size(); };
    uint32_t maxNumBvhNodes() const override { return (uint32_t)m_bvhNodes.size(); };
    void buildBvh() override{}; // Only necessary for dynamic objects
private:
    void loadFromFile(
        std::string_view fileName,
        const Transform& offset,
        std::optional<Material> overrideMaterial,
        UniqueTextureArray& textureArray);

    void addSubMesh(
        const aiScene* scene,
        unsigned meshIndex,
        const glm::mat4& transformMatrix,
        std::string_view texturePath,
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
