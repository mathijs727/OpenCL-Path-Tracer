#pragma once
#include "light.h"
#include "model/material.h"
#include "model/mesh.h"
#include "ray.h"
#include "shapes.h"
#include <gsl/gsl>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace raytracer {

struct SceneNode {
    SceneNode() = delete;
    SceneNode(SceneNode&&) = default;

    const SceneNode* parent;
    std::vector<std::unique_ptr<SceneNode>> children;
    AABB bounds;
    Transform transform;
    std::optional<uint32_t> meshID;
    std::optional<uint32_t> subBvhRootID;
};

struct MeshBvhPair {
    MeshBvhPair(std::shared_ptr<IMesh>& meshPtr, uint32_t bvhIndexOffset)
        : meshPtr(meshPtr)
        , bvhIndexOffset(bvhIndexOffset)
    {
    }
    std::shared_ptr<IMesh> meshPtr;
    uint32_t bvhIndexOffset; // Offset of meshes bvh into the global bvh array
};

class Scene {
public:
    friend class Bvh;

    Scene();
    ~Scene() = default;

    const SceneNode& addNode(const std::shared_ptr<IMesh> object, const Transform& transform = {}, SceneNode* parent = nullptr);

    void addLight(const Light& light);

    SceneNode& getRootNode() { return m_rootNode; }
    const SceneNode& getRootNode() const { return m_rootNode; }
    gsl::span<const Light> getLights() const { return m_lights; };
    gsl::span<MeshBvhPair> getMeshes() { return m_meshes; };

private:
    SceneNode m_rootNode;

    std::vector<Light> m_lights;
    std::unordered_map<const IMesh*, uint32_t> m_meshIDMapping;
    std::vector<MeshBvhPair> m_meshes;
};
}
