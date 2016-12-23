#include "scene.h"
#include <algorithm>
#include <iostream>
#include <crtdbg.h>

using namespace raytracer;

raytracer::SceneNode& raytracer::Scene::add_node(std::shared_ptr<Mesh> primitive, const Transform& transform, SceneNode* parent /*= nullptr*/) {
	//return add_node(std::vector<Mesh>{ primitive }, transform, parent );
	if (!parent) parent = &get_root_node();

	auto newChild = std::make_unique<SceneNode>();
	SceneNode& newChildRef = *newChild.get();
	newChild->transform = transform;
	newChild->parent = parent;	
	newChild->mesh = _meshes.size();
	_meshes.emplace_back(primitive, 0);
	parent->children.push_back(std::move(newChild));
	return newChildRef;
}