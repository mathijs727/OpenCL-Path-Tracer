#include "mesh.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <cassert>
#include <stack>
#include <iostream>
#include "template/includes.h"// Includes opencl

using namespace raytracer;

glm::mat4 remove_translation(const glm::mat4& mat) {
	glm::mat4 result = mat;
	result[3][0] = 0;
	result[3][1] = 0;
	result[3][2] = 0;
	return result;
}

glm::mat4 ai2glm(const aiMatrix4x4& t) {
	return glm::mat4(t.a1, t.a2, t.a3, t.a4, t.b1, t.b2, t.b3, t.b4, t.c1, t.c2, t.c3, t.c4, t.d1, t.d2, t.d3, t.d4 );
}

glm::vec3 ai2glm(const aiVector3D& v) {
	return glm::vec3(v.x,v.y,v.z);
}

void raytracer::Mesh::addData(aiMesh* in_mesh, const glm::mat4& transform_matrix) {
	uint vertex_starting_index = _vertices.size();
	for (uint v = 0; v < in_mesh->mNumVertices; ++v) {
		glm::vec4 vertex = transform_matrix * glm::vec4(ai2glm(in_mesh->mVertices[v]), 1);
		glm::vec4 normal = remove_translation(transform_matrix) * glm::vec4(ai2glm(in_mesh->mNormals[v]), 1);
		//std::cout << "importing vertex: " << position.x << ", " << position.y << ", " << position.z << std::endl;
		_vertices.push_back(vertex);
		_normals.push_back(normal);
	}
	for (uint f = 0; f < in_mesh->mNumFaces; ++f) {
		aiFace* in_face = &in_mesh->mFaces[f];
		if (in_face->mNumIndices != 3) {
			std::cout << "found a face which is not a triangle! discarding." << std::endl;
			continue;
		}
		auto indices = in_face->mIndices;
		auto face = glm::u32vec3(indices[0], indices[1], indices[2]) + vertex_starting_index;
		_faces.push_back(face);
		//std::cout << "importing face: " << indices[0] << ", " << indices[1] << ", " << indices[2] << ", starting index: " << vertex_starting_index << std::endl;
	}
}



Mesh raytracer::Mesh::LoadFromFile(const char* file, const Transform& offset) {
	struct StackElement
	{
		aiNode* node;
		glm::mat4x4 transform;
		StackElement(aiNode* node, const glm::mat4& transform = glm::mat4()) : node(node), transform(transform) {}
	};

	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(file,  aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_JoinIdenticalVertices | aiProcess_PreTransformVertices | aiProcess_ImproveCacheLocality);
	Mesh result;
	
	if (scene != nullptr && scene->mFlags != AI_SCENE_FLAGS_INCOMPLETE && scene->mRootNode != nullptr) {
		std::stack<StackElement> stack;
		stack.push(StackElement(scene->mRootNode, offset.matrix()));
		while (!stack.empty()) {
			std::cout << "loading .obj node..." << std::endl;
			auto current = stack.top();
			stack.pop();
			glm::mat4 cur_transform = current.transform * ai2glm(current.node->mTransformation);
			for (uint i = 0; i < current.node->mNumMeshes; ++i) result.addData(scene->mMeshes[i], cur_transform);
			for (uint i = 0; i < current.node->mNumChildren; ++i) stack.push(StackElement(current.node->mChildren[i], cur_transform));
		}
	}

	if (!result.isValid()) std::cout << "Mesh failed loading! reason: " << importer.GetErrorString() << std::endl;
	else std::cout << "Mesh imported! vertices: " << result._vertices.size() << ", indices: " << result._faces.size() << std::endl;
	return result;
}