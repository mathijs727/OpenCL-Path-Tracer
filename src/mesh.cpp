#include "mesh.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <cassert>
#include <stack>
#include <iostream>
#include "template/includes.h"// Includes opencl
#include "template/surface.h"
#include "allocator_singletons.h"
#include "sbvh.h"

using namespace raytracer;

inline glm::mat4 ai2glm(const aiMatrix4x4& t) {
	return glm::mat4(t.a1, t.a2, t.a3, t.a4, t.b1, t.b2, t.b3, t.b4, t.c1, t.c2, t.c3, t.c4, t.d1, t.d2, t.d3, t.d4);
}

inline glm::vec3 ai2glm(const aiVector3D& v) {
	return glm::vec3(v.x, v.y, v.z);
}

inline glm::vec3 ai2glm(const aiColor3D& c) {
	return glm::vec3(c.r, c.g, c.b);
}

inline glm::mat4 remove_translation(const glm::mat4& mat) {
	glm::mat4 result = mat;
	result[3][0] = 0;
	result[3][1] = 0;
	result[3][2] = 0;
	return result;
}

inline glm::mat4 normal_matrix(const glm::mat4& mat)
{
	return glm::transpose(glm::inverse(mat));
}

void raytracer::Mesh::addSubMesh(const aiScene* scene, uint mesh_index, const glm::mat4& transform_matrix) {
	aiMesh* in_mesh = scene->mMeshes[mesh_index];
	u32 vertex_starting_index = -1;

	if (in_mesh->mNumVertices == 0 || in_mesh->mNumFaces == 0)
		return;

	auto& materialAllocator = getMaterialAllocatorInstance();
	auto& vertexAllocator = getVertexAllocatorInstance();
	auto& triangleAllocator = getTriangleAllocatorInstance();
	u32 materialId = materialAllocator.allocate();

	// process the materials
	aiMaterial* material = scene->mMaterials[in_mesh->mMaterialIndex];
	aiColor3D colour; 
	material->Get(AI_MATKEY_COLOR_DIFFUSE, colour);
	if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
		aiString path;
		material->GetTexture(aiTextureType_DIFFUSE, 0, &path);
		materialAllocator.get(materialId) = Material::Diffuse(Texture(path.C_Str()), ai2glm(colour));
	}
	else {
		materialAllocator.get(materialId) = Material::Diffuse(ai2glm(colour));
	}

	// add all of the vertex data
	glm::mat4 normalMatrix = normal_matrix(transform_matrix);
	for (uint v = 0; v < in_mesh->mNumVertices; ++v) {
		glm::vec4 position = transform_matrix * glm::vec4(ai2glm(in_mesh->mVertices[v]), 1);
		glm::vec4 normal = normalMatrix * glm::vec4(ai2glm(in_mesh->mNormals[v]), 1);
		glm::vec2 texCoords;
		if (in_mesh->HasTextureCoords(0)) {
			texCoords.x = in_mesh->mTextureCoords[0][v].x;
			texCoords.y = in_mesh->mTextureCoords[0][v].y;
		}

		// Allocate a vertex
		u32 vertexId = vertexAllocator.allocate();
		if (vertex_starting_index == -1)
			vertex_starting_index = vertexId;

		// Fill in the vertex
		auto& vertex = vertexAllocator.get(vertexId);
		vertex.vertex = position;
		vertex.normal = normal;
		vertex.texCoord = texCoords;
		//std::cout << "importing vertex: " << position.x << ", " << position.y << ", " << position.z << std::endl;
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
		
		// Allocate a triangle
		u32 triangleIndex = triangleAllocator.allocate();
		auto& triangle = triangleAllocator.get(triangleIndex);
		if (_firstTriangleIndex == -1)
			_firstTriangleIndex = triangleIndex;

		// Fill in the triangle
		triangle.indices = face;
		triangle.material_index = materialId;

		_triangleCount++;
		//std::cout << "importing face: " << indices[0] << ", " << indices[1] << ", " << indices[2] << ", starting index: " << vertex_starting_index << std::endl;
	}
}

void raytracer::Mesh::loadFromFile(const char* file, const Transform& offset) {
	struct StackElement
	{
		aiNode* node;
		glm::mat4x4 transform;
		StackElement(aiNode* node, const glm::mat4& transform = glm::mat4()) : node(node), transform(transform) {}
	};

	_triangleCount = 0;
	_firstTriangleIndex = -1;
	_bvhRootNode = -1;

	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(file, aiProcessPreset_TargetRealtime_MaxQuality);

	if (scene != nullptr && scene->mFlags != AI_SCENE_FLAGS_INCOMPLETE && scene->mRootNode != nullptr) {
		std::stack<StackElement> stack;
		stack.push(StackElement(scene->mRootNode, offset.matrix()));
		while (!stack.empty()) {
			std::cout << "loading .obj node..." << std::endl;
			auto current = stack.top();
			stack.pop();
			glm::mat4 cur_transform = current.transform * ai2glm(current.node->mTransformation);
			for (uint i = 0; i < current.node->mNumMeshes; ++i) {
				addSubMesh(scene, current.node->mMeshes[i], cur_transform);
				//if (!success) std::cout << "Mesh failed loading! reason: " << importer.GetErrorString() << std::endl;
				//else std::cout << "Mesh imported! vertices: " << mesh._vertices.size() << ", indices: " << mesh._faces.size() << std::endl;
				//out_vec.push_back(mesh);
			}
			for (uint i = 0; i < current.node->mNumChildren; ++i) {
				stack.push(StackElement(current.node->mChildren[i], cur_transform));
			}
		}
	}

	// Create a BVH for the mesh
	SbvhBuilder bvhBuilder;
	_bvhRootNode = bvhBuilder.build(_firstTriangleIndex, _triangleCount);
}