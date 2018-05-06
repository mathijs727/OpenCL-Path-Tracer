#include "mesh.h"
#include "bvh/binned_bvh.h"
#include "bvh/fast_binned_bvh.h"
#include "bvh/sbvh.h"
#include "template/surface.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <cassert>
#include <fstream>
#include <iostream>
#include <stack>
#include <string>

static glm::mat4 ai2glm(const aiMatrix4x4& t)
{
    return glm::mat4(t.a1, t.a2, t.a3, t.a4, t.b1, t.b2, t.b3, t.b4, t.c1, t.c2, t.c3, t.c4, t.d1, t.d2, t.d3, t.d4);
}

static glm::vec3 ai2glm(const aiVector3D& v)
{
    return glm::vec3(v.x, v.y, v.z);
}

static glm::vec3 ai2glm(const aiColor3D& c)
{
    return glm::vec3(c.r, c.g, c.b);
}

static glm::mat4 normal_matrix(const glm::mat4& mat)
{
    return glm::transpose(glm::inverse(mat));
}

// http://stackoverflow.com/questions/3071665/getting-a-directory-name-from-a-filename
static std::string getPath(std::string_view str)
{
    size_t found;
    found = str.find_last_of("/\\");
    return std::string(str.substr(0, found)) + "/";
}

static bool fileExists(std::string_view name)
{
    std::ifstream f(name.data());
    return f.good() && f.is_open();
}

namespace raytracer {

Mesh::Mesh(std::string_view fileName, const Transform& offset, const Material& overrideMaterial, UniqueTextureArray& textureArray)
{
    loadFromFile(fileName, offset, overrideMaterial, textureArray);
}

Mesh::Mesh(std::string_view fileName, const Transform& offset, UniqueTextureArray& textureArray)
{
    loadFromFile(fileName, offset, {}, textureArray);
}

Mesh::Mesh(std::string_view fileName, const Material& overrideMaterial, UniqueTextureArray& textureArray)
{
    loadFromFile(fileName, {}, overrideMaterial, textureArray);
}

Mesh::Mesh(std::string_view fileName, UniqueTextureArray& textureArray)
{
    loadFromFile(fileName, {}, {}, textureArray);
}

void Mesh::addSubMesh(
    const aiScene* scene,
    unsigned mesh_index,
    const glm::mat4& transform_matrix,
    std::string_view texturePath,
    UniqueTextureArray& textureArray,
    std::optional<Material> overrideMaterial)
{
    const aiMesh* in_mesh = scene->mMeshes[mesh_index];

    if (in_mesh->mNumVertices == 0 || in_mesh->mNumFaces == 0)
        return;

    // process the materials
    uint32_t materialId = (uint32_t)m_materials.size();
    if (!overrideMaterial) {
        aiMaterial* material = scene->mMaterials[in_mesh->mMaterialIndex];
        aiColor3D colour;
        aiColor3D emissiveColour;
        material->Get(AI_MATKEY_COLOR_DIFFUSE, colour);
        material->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColour);
        bool emissive = emissiveColour.r != 0.0f || emissiveColour.g != 0.0f || emissiveColour.b != 0.0f;
        if (emissive) {
            m_materials.push_back(Material::Emissive(ai2glm(emissiveColour)));
        } else {
            if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
                aiString path;
                material->GetTexture(aiTextureType_DIFFUSE, 0, &path);
                std::string textureFile(texturePath);
                textureFile += path.C_Str();
                std::cout << "Texture path: " << textureFile << std::endl;
                m_materials.push_back(Material::Diffuse(textureArray.add(textureFile), ai2glm(colour)));
            } else {
                m_materials.push_back(Material::Diffuse(ai2glm(colour)));
            }
        }
    } else {
        m_materials.push_back(*overrideMaterial);
    }

    // add all of the vertex data
    glm::mat4 normalMatrix = normal_matrix(transform_matrix);
    uint32_t vertexOffset = (uint32_t)m_vertices.size();
    for (unsigned v = 0; v < in_mesh->mNumVertices; ++v) {
        glm::vec4 position = transform_matrix * glm::vec4(ai2glm(in_mesh->mVertices[v]), 1);
        glm::vec4 normal = glm::vec4(0);
        if (in_mesh->mNormals != nullptr)
            normal = normalMatrix * glm::vec4(ai2glm(in_mesh->mNormals[v]), 0);
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
        m_vertices.push_back(vertex);
    }

    // add all of the faces data
    for (unsigned f = 0; f < in_mesh->mNumFaces; ++f) {
        aiFace* in_face = &in_mesh->mFaces[f];
        if (in_face->mNumIndices != 3) {
            continue;
        }
        auto aiIndices = in_face->mIndices;
        auto face = glm::u32vec3(aiIndices[0], aiIndices[1], aiIndices[2]);

        // Fill in the triangle
        TriangleSceneData triangle;
        triangle.indices = face + vertexOffset;
        triangle.material_index = materialId;
        m_triangles.push_back(triangle);

        // Doesnt work if SBVH is gonna mess up triangle order anyways
        //if (emissive)
        //	_emissive_triangles.push_back((uint32_t)_triangles.size() - 1);
    }
}

void Mesh::collectEmissiveTriangles()
{
    m_emissiveTriangles.clear();
    for (uint32_t i = 0; i < (uint32_t)m_triangles.size(); i++) {
        auto triangle = m_triangles[i];
        auto material = m_materials[triangle.material_index];
        glm::vec4 vertices[3];
        vertices[0] = m_vertices[triangle.indices.x].vertex;
        vertices[1] = m_vertices[triangle.indices.y].vertex;
        vertices[2] = m_vertices[triangle.indices.z].vertex;
        if (material.type == Material::Type::EMISSIVE)
            m_emissiveTriangles.push_back(i);
    }
}

void Mesh::loadFromFile(
    std::string_view fileName,
    const Transform& offset,
    std::optional<Material> overrideMaterial,
    UniqueTextureArray& textureArray)
{
    struct StackElement {
        aiNode* node;
        glm::mat4x4 transform;
        StackElement(aiNode* node, const glm::mat4& transform = glm::mat4())
            : node(node)
            , transform(transform)
        {
        }
    };

    std::string path = getPath(fileName);

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(fileName.data(), aiProcessPreset_TargetRealtime_MaxQuality);

    if (scene == nullptr || scene->mRootNode == nullptr)
        std::cout << "Mesh not found: " << fileName << std::endl;

    if (scene != nullptr && scene->mFlags != AI_SCENE_FLAGS_INCOMPLETE && scene->mRootNode != nullptr) {
        std::stack<StackElement> stack;
        stack.push(StackElement(scene->mRootNode, offset.matrix()));
        while (!stack.empty()) {
            auto current = stack.top();
            stack.pop();
            glm::mat4 cur_transform = current.transform * ai2glm(current.node->mTransformation);
            for (unsigned i = 0; i < current.node->mNumMeshes; ++i) {
                addSubMesh(scene, current.node->mMeshes[i], cur_transform, path, textureArray, overrideMaterial);
                //if (!success) std::cout << "Mesh failed loading! reason: " << importer.GetErrorString() << std::endl;
                //else std::cout << "Mesh imported! vertices: " << mesh._vertices.size() << ", indices: " << mesh._faces.size() << std::endl;
                //out_vec.push_back(mesh);
            }
            for (unsigned i = 0; i < current.node->mNumChildren; ++i) {
                stack.push(StackElement(current.node->mChildren[i], cur_transform));
            }
        }
    }

    std::string bvhFileName(fileName);
    bvhFileName += ".bvh";
    bool buildBvh = true;
    if (fileExists(bvhFileName)) {
        std::cout << "Loading bvh from file: " << bvhFileName << std::endl;
        buildBvh = !loadBvh(bvhFileName.c_str());
    }

    if (buildBvh) {
        std::cout << "Starting bvh build..." << std::endl;
        // Create a BVH for the mesh
        BinnedBvhBuilder bvhBuilder;
        m_bvhRootNode = bvhBuilder.build(m_vertices, m_triangles, m_bvhNodes);

        std::cout << "Storing bvh in file: " << bvhFileName << std::endl;
        storeBvh(bvhFileName.c_str());
    }

    collectEmissiveTriangles();
}

const unsigned BVH_FILE_FORMAT_VERSION = 1;

void Mesh::storeBvh(const char* fileName)
{
    std::ofstream outFile;
    outFile.open(fileName, std::ios::out | std::ios::binary);

    // File format version
    outFile.write((char*)&BVH_FILE_FORMAT_VERSION, 4);

    // BVH root node index
    outFile.write((char*)&m_bvhRootNode, 4);

    // BVH nodes
    uint32_t numNodes = (uint32_t)m_bvhNodes.size();
    outFile.write((char*)&numNodes, 4);
    outFile.write((char*)m_bvhNodes.data(), numNodes * sizeof(SubBvhNode));

    // Triangles
    uint32_t numTriangles = (uint32_t)m_triangles.size();
    outFile.write((char*)&numTriangles, 4);
    outFile.write((char*)m_triangles.data(), numTriangles * sizeof(TriangleSceneData));

    outFile << std::endl;
    outFile.close();
}

bool Mesh::loadBvh(const char* fileName)
{
    std::string line;
    std::ifstream inFile;
    inFile.open(fileName, std::ios::binary);

    if (!inFile.is_open()) {
        std::cout << "Cant open bvh file" << std::endl;
        return false;
    }

    // Check file format version
    uint32_t formatVersion;
    inFile.read((char*)&formatVersion, 4);
    if (formatVersion != BVH_FILE_FORMAT_VERSION)
        return false;

    // Read root BVH node index
    inFile.read((char*)&m_bvhRootNode, 4);

    // Read BVH nodes
    uint32_t numNodes;
    inFile.read((char*)&numNodes, 4);
    m_bvhNodes.resize(numNodes);
    inFile.read((char*)m_bvhNodes.data(), numNodes * sizeof(SubBvhNode));

    // Read triangles
    uint32_t numTriangles;
    inFile.read((char*)&numTriangles, 4);
    m_triangles.resize(numTriangles);
    inFile.read((char*)m_triangles.data(), numTriangles * sizeof(TriangleSceneData));

    inFile.close();
    return true;
}
}