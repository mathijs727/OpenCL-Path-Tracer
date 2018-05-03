#pragma once
#include "bvh/binned_bvh.h"
#include "imesh.h"
#include "material.h"
#include "shapes.h"
#include "texture.h"
#include "transform.h"
#include "types.h"
#include "vertices.h"

#include <glm/glm.hpp>
#include <vector>

struct aiMesh;
struct aiScene;

namespace raytracer {

class Mesh : public IMesh {
public:
    Mesh() {}
    ~Mesh() {}

    Mesh(const char* file, UniqueTextureArray& textureArray, const Material& overrideMaterial, const Transform& offset = Transform())
    {
        loadFromFile(file, textureArray, overrideMaterial, offset);
    }

    Mesh(const char* file, UniqueTextureArray& textureArray, const Transform& offset = Transform())
    {
        loadFromFile(file, textureArray, offset);
    }

    void loadFromFile(
        const char* file,
        UniqueTextureArray& textureArray,
        const Transform& offset = Transform());

    void loadFromFile(
        const char* file,
        UniqueTextureArray& textureArray,
        const Material& overrideMaterial,
        const Transform& offset = Transform());

    const std::vector<VertexSceneData>& getVertices() const override { return _vertices; }
    const std::vector<TriangleSceneData>& getTriangles() const override { return _triangles; }
    const std::vector<Material>& getMaterials() const override { return _materials; }
    const std::vector<SubBvhNode>& getBvhNodes() const override { return _bvh_nodes; }
    const std::vector<u32>& getEmissiveTriangles() const override { return _emissive_triangles; }

    u32 getBvhRootNode() const override { return _bvh_root_node; };

    bool isDynamic() const override { return false; };
    u32 maxNumVertices() const override { return (u32)_vertices.size(); };
    u32 maxNumTriangles() const override { return (u32)_triangles.size(); };
    u32 maxNumMaterials() const override { return (u32)_materials.size(); };
    u32 maxNumBvhNodes() const override { return (u32)_bvh_nodes.size(); };
    void buildBvh() override{}; // Only necessary for dynamic objects
private:
    void loadFromFile(
        const char* file,
        UniqueTextureArray& textureArray,
        const Material* overrideMaterial,
        const Transform& offset);

    void addSubMesh(
        const aiScene* scene,
        uint mesh_index,
        const glm::mat4& transform_matrix,
        UniqueTextureArray& textureArray,
        const char* texturePath,
        const Material* overrideMaterial);
    void collectEmissiveTriangles();

    void storeBvh(const char* fileName);
    bool loadBvh(const char* fileName);

private:
    std::vector<VertexSceneData> _vertices;
    std::vector<TriangleSceneData> _triangles;
    std::vector<u32> _emissive_triangles;
    std::vector<Material> _materials;
    std::vector<SubBvhNode> _bvh_nodes;

    u32 _bvh_root_node;
};
}
