#pragma once
//#include "template/template.h"// Includes template/cl.hpp
#include "bvh/top_bvh.h"
#include "model/material.h"
#include "texture.h"
#include "vertices.h"
#include <memory>
#include <string_view>
#include <vector>

namespace Tmpl8 {
class Surface;
}

namespace raytracer {

class Camera;
class Scene;

class RayTracer {
public:
    RayTracer(int width, int height, std::shared_ptr<Scene> scene, const UniqueTextureArray& materialTextures, const UniqueTextureArray& skydomeTextures, GLuint outputTarget);
    ~RayTracer();

    void rayTrace(const Camera& camera);

    void frameTick(); // Load next animation frame data

    int getNumPasses();
    int getMaxPasses();

private:
    void setScene(std::shared_ptr<Scene> scene, const UniqueTextureArray& textureArray);
    void setSkydome(const UniqueTextureArray& skydomeTextureArray);
    void setTarget(GLuint glTexture);

    void traceRays(const Camera& camera);

    void accumulate(const Camera& camera);
    void clearAccumulationBuffer();
    void calculateAverageGrayscale();

    void copyNextAnimationFrameData();
    void collectTransformedLights(const SceneNode* node, const glm::mat4& transform);

    void initOpenCL();
    void initBuffers(
        uint32_t numVertices,
        uint32_t numTriangles,
        uint32_t numEmissiveTriangles,
        uint32_t numMaterials,
        uint32_t numSubBvhNodes,
        uint32_t numTopBvhNodes,
        uint32_t numLights);

    cl::Kernel loadKernel(std::string_view fileName, std::string_view funcName);

private:
    CLContext m_clContext;

    std::shared_ptr<Scene> m_scene;
    std::unique_ptr<CLTextureArray> m_skydomeTextures;
    std::unique_ptr<CLTextureArray> m_materialTextures;

    cl_uint m_screenWidth, m_screenHeight;
    GLuint m_glOutputImage;
    cl::Image2D m_clOutputImage;
    std::unique_ptr<float[]> m_cpuOutputImage;

    cl::Kernel m_generateRaysKernel;
    cl::Kernel m_intersectWalkKernel;
    cl::Kernel m_shadingKernel;
    cl::Kernel m_intersectShadowsKernel;
    cl::Kernel m_updateKernelDataKernel;

    cl::Buffer m_rayTraversalBuffer;
    cl::Buffer m_kernelDataBuffer;
    cl::Buffer m_randomStreamBuffer;
    cl::Buffer m_raysBuffer[2];
    cl::Buffer m_shadingRequestBuffer;
    cl::Buffer m_shadowRaysBuffer;

    cl_uint m_samplesPerPixel;
    cl::Kernel m_accumulateKernel;
    cl::Buffer m_accumulationBuffer;
    cl::ImageGL m_clGLInteropOutputImage;

    std::vector<VertexSceneData> m_verticesHost;
    std::vector<TriangleSceneData> m_trianglesHost;
    std::vector<EmissiveTriangle> m_emissiveTrianglesHost;
    std::vector<Material> m_materialsHost;
    std::vector<TopBvhNode> m_topBvhNodesHost;
    std::vector<SubBvhNode> m_subBvhNodesHost;

    unsigned m_activeBuffer = 0;
    cl_uint m_numStaticVertices;
    cl_uint m_numStaticTriangles;
    cl_uint m_numEmissiveTriangles[2];
    cl_uint m_numStaticMaterials;
    cl_uint m_numStaticBvhNodes;
    cl_uint m_topBvhRootNode[2];

    cl::Buffer m_verticesBuffers[2];
    cl::Buffer m_trianglesBuffers[2];
    cl::Buffer m_emissiveTrianglesBuffers[2];
    cl::Buffer m_materialsBuffers[2];
    cl::Buffer m_topBvhBuffers[2];
    cl::Buffer m_subBvhBuffers[2];
};
}
