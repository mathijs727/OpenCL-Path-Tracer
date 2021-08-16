#include "scene.h"
#include <algorithm>
#include <iostream>
#include <limits>

namespace raytracer {

Scene::Scene()
    : m_rootNode()
{
}

const SceneNode& Scene::addNode(std::shared_ptr<IMesh> object, const Transform& transform, SceneNode* parent)
{
    if (!parent)
        parent = &m_rootNode;

    parent->bounds.fit(object->getBounds());

    uint32_t meshID;
    if (auto iter = m_meshIDMapping.find(object.get()); iter != m_meshIDMapping.end()) {
        meshID = iter->second;
    } else {
        meshID = (uint32_t)m_meshes.size();
        m_meshes.emplace_back(object, object->getBvhRootNode());
        m_meshIDMapping[object.get()] = meshID;
    }

    auto newChild = std::make_unique<SceneNode>(SceneNode{ parent,
        {},
        object->getBounds(),
        transform,
        meshID,
        object->getBvhRootNode() });
    SceneNode& newChildRef = *newChild.get();
    parent->children.push_back(std::move(newChild));
    return newChildRef;
}

}
