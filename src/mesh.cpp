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
#include <fstream>

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

inline bool fileExists(const std::string& name) {
	std::ifstream f(name.c_str());
	return f.good() && f.is_open();
}


void raytracer::Mesh::addSubMesh(
	const aiScene* scene,
	uint mesh_index,
	const glm::mat4& transform_matrix,
	const char* texturePath) {
	aiMesh* in_mesh = scene->mMeshes[mesh_index];

	if (in_mesh->mNumVertices == 0 || in_mesh->mNumFaces == 0)
		return;

	// process the materials
	u32 materialId = (u32)_materials.size();
	aiMaterial* material = scene->mMaterials[in_mesh->mMaterialIndex];
	aiColor3D colour;
	aiColor3D emmisiveColour;
	material->Get(AI_MATKEY_COLOR_DIFFUSE, colour);
	material->Get(AI_MATKEY_COLOR_EMISSIVE, emmisiveColour);
	bool emmisive = emmisiveColour.r != 0.0f || emmisiveColour.g != 0.0f || emmisiveColour.b != 0.0f;
	if (emmisive)
	{
		_materials.push_back(Material::Emmisive(ai2glm(emmisiveColour)));
	}
	else {
		if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
			aiString path;
			material->GetTexture(aiTextureType_DIFFUSE, 0, &path);
			std::string textureFile = texturePath;
			textureFile += path.C_Str();
			//_materials.push_back(Material::Diffuse(ai2glm(colour)));
			_materials.push_back(Material::Diffuse(Texture(textureFile.c_str()), ai2glm(colour)));
		}
		else {
			_materials.push_back(Material::Diffuse(ai2glm(colour)));
		}
	}

	// add all of the vertex data
	glm::mat4 normalMatrix = normal_matrix(transform_matrix);
	u32 vertexOffset = (u32)_vertices.size();
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

		// Doesnt work if SBVH is gonna mess up triangle order anyways
		//if (emmisive)
		//	_emmisive_triangles.push_back((u32)_triangles.size() - 1);
	}
}

void raytracer::Mesh::collectEmmisiveTriangles()
{
	_emmisive_triangles.clear();
	for (u32 i = 0; i < _triangles.size(); i++)
	{
		auto triangle = _triangles[i];
		auto material = _materials[triangle.material_index];
		glm::vec4 vertices[3];
		vertices[0] = _vertices[triangle.indices.x].vertex;
		vertices[1] = _vertices[triangle.indices.y].vertex;
		vertices[2] = _vertices[triangle.indices.z].vertex;
		if (material.type == Material::Type::Emmisive)
			_emmisive_triangles.push_back(i);
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

	std::string bvhFileName = file;
	bvhFileName += ".bvh";
	bool buildBvh = true;
	if (fileExists(bvhFileName))
	{
		std::cout << "Loading bvh from file: " << bvhFileName << std::endl;
		//buildBvh = !loadBvh(bvhFileName.c_str());
	}

	if (buildBvh) {
		std::cout << "Starting bvh build..." << std::endl;
		// Create a BVH for the mesh
		SbvhBuilder bvhBuilder;
		_bvh_root_node = bvhBuilder.build(_vertices, _triangles, _bvh_nodes);

		std::cout << "Storing bvh in file: " << bvhFileName << std::endl;
		storeBvh(bvhFileName.c_str());
	}

	collectEmmisiveTriangles();
}



const u32 BVH_FILE_FORMAT_VERSION = 1;

void raytracer::Mesh::storeBvh(const char* fileName)
{
	std::ofstream outFile;
	outFile.open(fileName, std::ios::out | std::ios::binary);

	// File format version
	outFile.write((char*)&BVH_FILE_FORMAT_VERSION, 4);

	// BVH root node index
	outFile.write((char*)&_bvh_root_node, 4);

	// BVH nodes
	u32 numNodes = (u32)_bvh_nodes.size();
	outFile.write((char*)&numNodes, 4);
	outFile.write((char*)_bvh_nodes.data(), numNodes * sizeof(SubBvhNode));

	// Triangles
	u32 numTriangles = (u32)_triangles.size();
	outFile.write((char*)&numTriangles, 4);
	outFile.write((char*)_triangles.data(), numTriangles * sizeof(TriangleSceneData));

	outFile << std::endl;
	outFile.close();
}

bool raytracer::Mesh::loadBvh(const char* fileName)
{
	std::string line;
	std::ifstream inFile;
	inFile.open(fileName, std::ios::binary);
	
	if (!inFile.is_open())
	{
		std::cout << "Cant open bvh file" << std::endl;
		return false;
	}

	// Check file format version
	u32 formatVersion;
	inFile.read((char*)&formatVersion, 4);
	if (formatVersion != BVH_FILE_FORMAT_VERSION)
		return false;

	// Read root BVH node index
	inFile.read((char*)&_bvh_root_node, 4);

	// Read BVH nodes
	u32 numNodes;
	inFile.read((char*)&numNodes, 4);
	_bvh_nodes.resize(numNodes);
	inFile.read((char*)_bvh_nodes.data(), numNodes * sizeof(SubBvhNode));

	// Read triangles
	u32 numTriangles;
	inFile.read((char*)&numTriangles, 4);
	_triangles.resize(numTriangles);
	inFile.read((char*)_triangles.data(), numTriangles * sizeof(TriangleSceneData));

	inFile.close();
	return true;
}