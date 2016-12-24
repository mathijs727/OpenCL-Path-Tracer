#include "mesh.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <cassert>
#include <stack>
#include <iostream>
#include "template/includes.h"// Includes opencl
#include "template/surface.h"
#include "sbvh.h"
#include <string>

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

inline glm::mat4 normal_matrix(const glm::mat4& mat)
{
	return glm::transpose(glm::inverse(mat));
}

// http://stackoverflow.com/questions/3071665/getting-a-directory-name-from-a-filename
inline std::string getPath(const std::string& str)
{
	size_t found;
	found = str.find_last_of("/\\");
	return str.substr(0, found) + "/";
}



void raytracer::Mesh::addSubMesh(
	const aiScene* scene,
	uint mesh_index,
	const glm::mat4& transform_matrix,
	const char* texturePath) {
	aiMesh* in_mesh = scene->mMeshes[mesh_index];

	if (in_mesh->mNumVertices == 0 || in_mesh->mNumFaces == 0)
		return;

	std::cout << "Loading " << in_mesh->mNumVertices << " vertices" << std::endl;
	std::cout << "Loading " << in_mesh->mNumFaces << " triangles" << std::endl;

	// process the materials
	u32 materialId = _materials.size();
	aiMaterial* material = scene->mMaterials[in_mesh->mMaterialIndex];
	aiColor3D colour; 
	material->Get(AI_MATKEY_COLOR_DIFFUSE, colour);
	if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
		aiString path;
		material->GetTexture(aiTextureType_DIFFUSE, 0, &path);
		std::string textureFile = texturePath;
		textureFile += path.C_Str();
		_materials.push_back(Material::Diffuse(Texture(textureFile.c_str()), ai2glm(colour)));
	}
	else {
		_materials.push_back(Material::Diffuse(ai2glm(colour)));
	}

	// add all of the vertex data
	glm::mat4 normalMatrix = normal_matrix(transform_matrix);
	u32 vertexOffset = _vertices.size();
	for (uint v = 0; v < in_mesh->mNumVertices; ++v) {
		glm::vec4 position = transform_matrix * glm::vec4(ai2glm(in_mesh->mVertices[v]), 1);
		glm::vec4 normal = normalMatrix * glm::vec4(ai2glm(in_mesh->mNormals[v]), 1);
		glm::vec2 texCoords;
		if (in_mesh->HasTextureCoords(0)) {
			texCoords.x = in_mesh->mTextureCoords[0][v].x;
			texCoords.y = in_mesh->mTextureCoords[0][v].y;
		}

		// Fill in the vertex
		VertexSceneData vertex;
		vertex.vertex = position;
		vertex.normal = normal;
		vertex.texCoord = texCoords;
		_vertices.push_back(vertex);
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
		auto face = glm::u32vec3(aiIndices[0], aiIndices[1], aiIndices[2]);

		// Fill in the triangle
		TriangleSceneData triangle;
		triangle.indices = face + vertexOffset;
		triangle.material_index = materialId;
		_triangles.push_back(triangle);

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

	std::string path = getPath(file);

	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(file, aiProcessPreset_TargetRealtime_MaxQuality);

	if (scene == nullptr || scene->mRootNode == nullptr)
		std::cout << "Mesh not found: " << file << std::endl;

	if (scene != nullptr && scene->mFlags != AI_SCENE_FLAGS_INCOMPLETE && scene->mRootNode != nullptr) {
		std::stack<StackElement> stack;
		stack.push(StackElement(scene->mRootNode, offset.matrix()));
		while (!stack.empty()) {
			auto current = stack.top();
			stack.pop();
			glm::mat4 cur_transform = current.transform * ai2glm(current.node->mTransformation);
			for (uint i = 0; i < current.node->mNumMeshes; ++i) {
				addSubMesh(scene, current.node->mMeshes[i], cur_transform, path.c_str());
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
	_bvh_root_node = bvhBuilder.build(_vertices, _triangles, _bvh_nodes);
}