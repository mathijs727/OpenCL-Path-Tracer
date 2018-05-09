#include "scene.h"
#include <algorithm>
#include <iostream>
#include <limits>

namespace raytracer {

Scene::Scene()
    : m_rootNode({ nullptr, {}, {}, std::numeric_limits<uint32_t>::max() })
{
}

const SceneNode& Scene::addNode(std::shared_ptr<IMesh> primitive, const Transform& transform, SceneNode* parent)
{
    if (!parent)
        parent = &m_rootNode;

    bool found = false;
    uint32_t meshID = 0;
    for (auto& pair : m_meshes) {
        if (pair.meshID.get() == primitive.get()) {
            found = true;
            break;
        }
        meshID++;
    }

    if (!found) {
        meshID = (uint32_t)m_meshes.size();
        m_meshes.emplace_back(primitive, 0);
    }

    auto newChild = std::unique_ptr<SceneNode>(new SceneNode{
        parent,
        {},
        transform,
        meshID });
    SceneNode& newChildRef = *newChild.get();
    //newChild->transform = transform;
    //newChild->parent = parent;
    //newChild->mesh = meshId;
    parent->children.push_back(std::move(newChild));
    return newChildRef;
}

void Scene::addLight(const Light& light)
{
    m_lights.push_back(light);
}
}
