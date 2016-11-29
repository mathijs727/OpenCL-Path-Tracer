#include "mesh.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <cassert>
#include <stack>
#include <iostream>
#include <cl/cl.h>
using namespace raytracer;

void raytracer::Mesh::addData(aiMesh* in_mesh) {
	for (int v = 0; v < in_mesh->mNumVertices; ++v) {
		auto position = in_mesh->mVertices[v];
		glm::vec3 vertex(position.x, position.y, position.z);
		_vertices.push_back(vertex);
	}
	for (int f = 0; f < in_mesh->mNumFaces; ++f) {
		aiFace* in_face = in_mesh->mFaces;
		assert(in_face->mNumIndices == 3);
		glm::u32vec3 face;
		memcpy(&face, &in_face->mIndices, 3*sizeof(u32));
		_triangleIndices.push_back(face);

		cl_float3 vector;
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