#pragma once
#include "bvh/binned_bvh.h"
#include "imesh.h"
#include "material.h"
#include "shapes.h"
#include "texture.h"
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

    const std::vector<VertexSceneData>& getVertices() const override { return m_vertices; }
    const std::vector<TriangleSceneData>& getTriangles() const override { return m_triangles; }
    const std::vector<Material>& getMaterials() const override { return m_materials; }
    const std::vector<SubBvhNode>& getBvhNodes() const override { return m_bvhNodes; }
    const std::vector<uint32_t>& getEmissiveTriangles() const override { return m_emissiveTriangles; }

    uint32_t getBvhRootNode() const override { return m_bvhRootNode; };

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
        unsigned mesh_index,
        const glm::mat4& transform_matrix,
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
    std::vector<SubBvhNode> m_bvhNodes;

    uint32_t m_bvhRootNode;
};
}
