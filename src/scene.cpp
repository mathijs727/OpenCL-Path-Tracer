#include "scene.h"
#include <algorithm>
#include <iostream>

namespace raytracer {

SceneNode& Scene::addNode(std::shared_ptr<IMesh> primitive, const Transform& transform, SceneNode* parent)
{
    if (!parent)
        parent = &getRootNode();

    bool found = false;
    uint32_t meshId = 0;
    for (auto& pair : m_meshes) {
        if (pair.mesh.get() == primitive.get()) {
            found = true;
            break;
        }
        meshId++;
    }

    if (!found) {
        meshId = (uint32_t)m_meshes.size();
        m_meshes.emplace_back(primitive, 0);
    }

    auto newChild = std::make_unique<SceneNode>();
    SceneNode& newChildRef = *newChild.get();
    newChild->transform = transform;
    newChild->parent = parent;
    newChild->mesh = meshId;
    parent->children.push_back(std::move(newChild));
    return newChildRef;
}

void Scene::addLight(const Light& light)
{
    m_lights.push_back(light);
}

}
