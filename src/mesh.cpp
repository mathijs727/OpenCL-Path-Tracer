#include "mesh.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <cassert>
#include <stack>
#include <iostream>
#include "template/includes.h"// Includes opencl
#include "material.h"
#include "template/surface.h"

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

glm::vec3 ai2glm(const aiColor3D& c) {
	return glm::vec3(c.r, c.g, c.b);
}

Mesh raytracer::Mesh::makeMesh(const aiScene* scene, uint mesh_index, const glm::mat4& transform_matrix) {
	aiMesh* in_mesh = scene->mMeshes[mesh_index];
	Mesh result;
	uint vertex_starting_index = 0;
	// reserve the correct space for empty vectors;
	result._vertices.reserve(in_mesh->mNumVertices);
	result._normals.reserve(in_mesh->mNumVertices);
	result._textureCoords.reserve(in_mesh->mNumVertices);
	result._faces.reserve(in_mesh->mNumFaces);

	// process the materials
	aiMaterial* material = scene->mMaterials[in_mesh->mMaterialIndex];
	aiColor3D colour; 
	material->Get(AI_MATKEY_COLOR_DIFFUSE, colour);
	if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
		aiString path;
		material->GetTexture(aiTextureType_DIFFUSE, 0, &path);
		result._material = Material::Diffuse(Texture(path.C_Str()), ai2glm(colour));
	}
	else {
		result._material = Material::Diffuse(ai2glm(colour));
	}

	// add all of the vertex data
	for (uint v = 0; v < in_mesh->mNumVertices; ++v) {
		glm::vec4 vertex = transform_matrix * glm::vec4(ai2glm(in_mesh->mVertices[v]), 1);
		glm::vec4 normal = remove_translation(transform_matrix) * glm::vec4(ai2glm(in_mesh->mNormals[v]), 1);
		glm::vec2 texCoords;
		if (in_mesh->HasTextureCoords(0)) {
			texCoords.x = in_mesh->mTextureCoords[0][v].x;
			texCoords.y = in_mesh->mTextureCoords[0][v].y;
		}
		//std::cout << "importing vertex: " << position.x << ", " << position.y << ", " << position.z << std::endl;
		result._vertices.push_back(vertex);
		result._normals.push_back(normal);
		result._textureCoords.push_back(texCoords);
	}

	// add all of the faces data
	for (uint f = 0; f < in_mesh->mNumFaces; ++f) {
		aiFace* in_face = &in_mesh->mFaces[f];
		if (in_face->mNumIndices != 3) {
			std::cout << "found a face which is not a triangle! discarding." << std::endl;
			continue;
		}
		auto aiIndices = in_face->mIndices;
		auto face = glm::u32vec3(aiIndices[0], aiIndices[1], aiIndices[2]) + vertex_starting_index;
		result._faces.push_back(face);
		//std::cout << "importing face: " << indices[0] << ", " << indices[1] << ", " << indices[2] << ", starting index: " << vertex_starting_index << std::endl;
	}

	return result;
}



std::vector<Mesh> raytracer::Mesh::LoadFromFile(const char* file, const Transform& offset) {
	struct StackElement
	{
		aiNode* node;
		glm::mat4x4 transform;
		StackElement(aiNode* node, const glm::mat4& transform = glm::mat4()) : node(node), transform(transform) {}
	};

	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(file, aiProcessPreset_TargetRealtime_MaxQuality);
	std::vector<Mesh> result;
	
	if (scene != nullptr && scene->mFlags != AI_SCENE_FLAGS_INCOMPLETE && scene->mRootNode != nullptr) {
		std::stack<StackElement> stack;
		stack.push(StackElement(scene->mRootNode, offset.matrix()));
		while (!stack.empty()) {
			std::cout << "loading .obj node..." << std::endl;
			auto current = stack.top();
			stack.pop();
			glm::mat4 cur_transform = current.transform * ai2glm(current.node->mTransformation);
			for (uint i = 0; i < current.node->mNumMeshes; ++i) {
				Mesh mesh = makeMesh(scene, current.node->mMeshes[i], cur_transform);
				if (!mesh.isValid()) std::cout << "Mesh failed loading! reason: " << importer.GetErrorString() << std::endl;
				else std::cout << "Mesh imported! vertices: " << mesh._vertices.size() << ", indices: " << mesh._faces.size() << std::endl;
				result.push_back(mesh);
			}
			for (uint i = 0; i < current.node->mNumChildren; ++i) { 
				stack.push(StackElement(current.node->mChildren[i], cur_transform));
			}
		}
	}
	
	return result;
}