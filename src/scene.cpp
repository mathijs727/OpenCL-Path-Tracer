#include "scene.h"
#include <algorithm>
#include <iostream>
#include <crtdbg.h>

using namespace raytracer;

raytracer::SceneNode& raytracer::Scene::add_node(std::shared_ptr<IMesh> primitive, const Transform& transform, SceneNode* parent /*= nullptr*/) {
	//return add_node(std::vector<Mesh>{ primitive }, transform, parent );
	if (!parent) parent = &get_root_node();

	bool found = false;
	u32 meshId = 0;
	for (auto& pair : _meshes)
	{
		if (pair.mesh.get() == primitive.get())
		{
			found = true;
			break;
		}
		meshId++;
	}

	if (!found)
	{
		meshId = (u32)_meshes.size();
		_meshes.emplace_back(primitive, 0);
	}

	auto newChild = std::make_unique<SceneNode>();
	SceneNode& newChildRef = *newChild.get();
	newChild->transform = transform;
	newChild->parent = parent;	
	newChild->mesh = meshId;
	parent->children.push_back(std::move(newChild));
	return newChildRef;
}