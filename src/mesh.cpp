#include "mesh.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <cassert>
#include <stack>
#include <iostream>

using namespace raytracer;

void raytracer::Mesh::addData(aiMesh* in_mesh) {
	for (int v = 0; v < in_mesh->mNumVertices; ++v) {
		auto position = in_mesh->mVertices[v];
		Vertex vertex;
		vertex.point = glm::vec3(position.x, position.y, position.z);
		_vertices.push_back(vertex);
	}
	for (int f = 0; f < in_mesh->mNumFaces; ++f) {
		aiFace* in_face = in_mesh->mFaces;
		assert(in_face->mNumIndices == 3);
		TriangleIndexes face;
		face.first = in_face->mIndices[0];
		face.second = in_face->mIndices[1];
		face.third = in_face->mIndices[2];
		_triangleIndices.push_back(face);
	}
}

Mesh raytracer::Mesh::LoadFromFile(const char* file) {

	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(file, aiProcess_Triangulate | aiProcess_GenNormals);
	Mesh result;
	
	if (scene != nullptr && scene->mFlags != AI_SCENE_FLAGS_INCOMPLETE && scene->mRootNode != nullptr) {
		std::stack<aiNode*> stack;
		stack.push(scene->mRootNode);
		while (!stack.empty()) {
			std::cout << "loading .obj node..." << std::endl;
			auto current = stack.top();
			stack.pop();
			for (int i = 0; i < current->mNumMeshes; ++i) result.addData(scene->mMeshes[i]);
			for (int i = 0; i < current->mNumChildren; ++i) stack.push(current->mChildren[i]);
		}
	}

	if (!result.isValid()) std::cout << "Mesh failed loading! reason: " << importer.GetErrorString() << std::endl;
	return result;
}