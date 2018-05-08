#pragma once
#include "light.h"
#include "model/material.h"
#include "model/mesh.h"
#include "ray.h"
#include "shapes.h"
#include <gsl/gsl>
#include <memory>
#include <vector>

namespace raytracer {

struct SceneNode {
    SceneNode* parent = nullptr;
    std::vector<std::unique_ptr<SceneNode>> children;
    Transform transform;
    uint32_t mesh = -1;
};

struct MeshBvhPair {
    MeshBvhPair(std::shared_ptr<IMesh>& mesh, uint32_t offset)
        : mesh(mesh)
        , bvhOffset(offset)
    {
    }
    std::shared_ptr<IMesh> mesh;
    uint32_t bvhOffset; // Offset of meshes bvh into the global bvh array
};

class Scene {
public:
    friend class Bvh;

    Scene() = default;
    ~Scene() = default;

    SceneNode& addNode(const std::shared_ptr<IMesh> primitive, const Transform& transform = {}, SceneNode* parent = nullptr);

    void addLight(const Light& light);

    SceneNode& getRootNode() { return m_rootNode; }
    gsl::span<const Light> getLights() const { return m_lights; };
    gsl::span<MeshBvhPair> getMeshes() { return m_meshes; };

private:
    SceneNode m_rootNode;

    std::vector<Light> m_lights;
    std::vector<MeshBvhPair> m_meshes;
};
}
