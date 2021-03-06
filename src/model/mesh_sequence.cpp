#include "mesh_sequence.h"
#include "bvh/bvh_build.h"
#include "bvh/refit_bvh.h"
#include "mesh_helpers.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <fstream>
#include <iostream>
#include <stack>
#include <string>

namespace raytracer {

MeshSequence::MeshSequence(std::string_view fileName, bool refitting, UniqueTextureArray& textureArray)
    : m_refitting(refitting)
{
    loadFile(fileName, {}, textureArray);
}

MeshSequence::MeshSequence(std::string_view fileName, const Transform& transform, bool refitting, UniqueTextureArray& textureArray)
    : m_refitting(refitting)
{
    loadFile(fileName, transform, textureArray);
}

MeshSequence::MeshSequence(gsl::span<std::string_view> files, bool refitting, UniqueTextureArray& textureArray)
    : m_refitting(refitting)
{
    loadFiles(files, {}, textureArray);
}

MeshSequence::MeshSequence(gsl::span<std::string_view> files, const Transform& transform, bool refitting, UniqueTextureArray& textureArray)
    : m_refitting(refitting)
{
    loadFiles(files, transform, textureArray);
}

void MeshSequence::goToNextFrame()
{
    m_currentFrame = (m_currentFrame + 1) % m_frames.size();
    m_bvhNeedsUpdate = true;
}

uint32_t MeshSequence::maxNumVertices() const
{
    uint32_t max = 0;
    for (auto& frame : m_frames) {
        max = std::max(max, (uint32_t)frame.vertices.size());
    }
    return max;
}

uint32_t MeshSequence::maxNumTriangles() const
{
    uint32_t max = 0;
    for (auto& frame : m_frames) {
        max = std::max(max, (uint32_t)frame.triangles.size());
    }
    return max;
}

uint32_t MeshSequence::maxNumMaterials() const
{
    uint32_t max = 0;
    for (auto& frame : m_frames) {
        max = std::max(max, (uint32_t)frame.materials.size());
    }
    return max;
}

uint32_t MeshSequence::maxNumBvhNodes() const
{
    uint32_t max = 0;
    for (auto& frame : m_frames) {
        max = std::max(max, (uint32_t)frame.triangles.size());
    }
    return max;
}

void MeshSequence::buildBvh()
{
    if (!m_bvhNeedsUpdate)
        return;
    m_bvhNeedsUpdate = false;

    MeshFrame& frame = m_frames[m_currentFrame];
    if (m_refitting) {
        refitBVH(m_bvhNodes, m_bvhRootNode, frame.vertices, frame.triangles);
    } else {
        // Clear bvh nodes list
        m_bvhNodes.clear();

        // Create a new bvh using the fast binned builder
        std::tie(m_bvhRootNode, frame.triangles, m_bvhNodes) = buildBinnedFastBVH(frame.vertices, frame.triangles);
    }
}

void MeshSequence::addSubMesh(
    const aiScene& scene,
    unsigned meshIndex,
    const glm::mat4& transformMatrix,
    std::string_view texturePath,
    UniqueTextureArray& textureArray,
    std::vector<VertexSceneData>& outVertices,
    std::vector<TriangleSceneData>& outTriangles,
    std::vector<Material>& outMaterials)
{
    const aiMesh* inMesh = scene.mMeshes[meshIndex];

    if (inMesh->mNumVertices == 0 || inMesh->mNumFaces == 0)
        return;

    // process the materials
    uint32_t materialId = (uint32_t)outMaterials.size();
    const aiMaterial* material = scene.mMaterials[inMesh->mMaterialIndex];
    aiColor3D colour;
    material->Get(AI_MATKEY_COLOR_DIFFUSE, colour);
    if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
        aiString path;
        material->GetTexture(aiTextureType_DIFFUSE, 0, &path);
        std::string textureFile(texturePath);
        textureFile += path.C_Str();
        outMaterials.push_back(Material::Diffuse(textureArray.add(textureFile), ai2glm(colour)));
    } else {
        outMaterials.push_back(Material::Diffuse(ai2glm(colour)));
    }

    // add all of the vertex data
    glm::mat4 normalMatrix = toNormalMatrix(transformMatrix);
    uint32_t vertexOffset = (uint32_t)outVertices.size();
    for (unsigned v = 0; v < inMesh->mNumVertices; ++v) {
        glm::vec4 position = transformMatrix * glm::vec4(ai2glm(inMesh->mVertices[v]), 1);
        glm::vec4 normal = normalMatrix * glm::vec4(ai2glm(inMesh->mNormals[v]), 1);
        glm::vec2 texCoords;
        if (inMesh->HasTextureCoords(0)) {
            texCoords.x = inMesh->mTextureCoords[0][v].x;
            texCoords.y = inMesh->mTextureCoords[0][v].y;
        }

        // Fill in the vertex
        VertexSceneData vertex;
        vertex.vertex = position;
        vertex.normal = normal;
        vertex.texCoord = texCoords;
        outVertices.push_back(vertex);
    }

    // add all of the faces data
    for (unsigned f = 0; f < inMesh->mNumFaces; ++f) {
        aiFace* inFace = &inMesh->mFaces[f];
        if (inFace->mNumIndices != 3) {
            std::cout << "found a face which is not a triangle! discarding." << std::endl;
            continue;
        }
        auto aiIndices = inFace->mIndices;
        auto face = glm::u32vec3(aiIndices[0], aiIndices[1], aiIndices[2]);

        // Fill in the triangle
        TriangleSceneData triangle;
        triangle.indices = face + vertexOffset;
        triangle.materialIndex = materialId;
        outTriangles.push_back(triangle);

        //std::cout << "importing face: " << indices[0] << ", " << indices[1] << ", " << indices[2] << ", starting index: " << vertex_starting_index << std::endl;
    }
}

void MeshSequence::loadFile(std::string_view fileName, const Transform& offset, UniqueTextureArray& textureArray)
{
    struct StackElement {
        aiNode* node;
        glm::mat4x4 transform;
        StackElement(aiNode* node, const glm::mat4& transform = glm::mat4(1.0f))
            : node(node)
            , transform(transform)
        {
        }
    };

    m_frames.emplace_back();
    MeshFrame& frame = m_frames.back();

    std::string path = getPath(fileName);

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(fileName.data(), aiProcessPreset_TargetRealtime_MaxQuality);

    if (scene && scene->mFlags != AI_SCENE_FLAGS_INCOMPLETE && scene->mRootNode != nullptr) {
        std::stack<StackElement> stack;
        stack.push(StackElement(scene->mRootNode, offset.matrix()));
        while (!stack.empty()) {
            auto current = stack.top();
            stack.pop();
            glm::mat4 currentTransform = current.transform * ai2glm(current.node->mTransformation);
            for (unsigned i = 0; i < current.node->mNumMeshes; ++i) {
                addSubMesh(
                    *scene,
                    current.node->mMeshes[i],
                    currentTransform,
                    path,
                    textureArray,
                    frame.vertices,
                    frame.triangles,
                    frame.materials);
                //if (!success) std::cout << "Mesh failed loading! reason: " << importer.GetErrorString() << std::endl;
                //else std::cout << "Mesh imported! vertices: " << mesh._vertices.size() << ", indices: " << mesh._faces.size() << std::endl;
                //out_vec.push_back(mesh);
            }
            for (unsigned i = 0; i < current.node->mNumChildren; ++i) {
                stack.push(StackElement(current.node->mChildren[i], currentTransform));
            }
        }
    }
}

void MeshSequence::loadFiles(gsl::span<std::string_view> files, const Transform& offset, UniqueTextureArray& textureArray)
{

    for (auto fileName : files) {
        loadFile(fileName, offset, textureArray);
    }

    if (m_refitting) {
        auto& f0 = m_frames[0];
        //SbvhBuilder builder;
        //m_bvhRootNode = builder.build(f0.vertices, f0.triangles, m_bvhNodes);
        std::tie(m_bvhRootNode, f0.triangles, m_bvhNodes) = buildSpatialSplitBVH(f0.vertices, f0.triangles);
    }
}
}
