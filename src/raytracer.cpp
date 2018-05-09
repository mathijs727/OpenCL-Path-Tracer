#include "raytracer.h"

#include "camera.h"
#include "opencl/cl_gl_includes.h"
#include "opencl/cl_helpers.h"
#include "pixel.h"
#include "ray.h"
#include "scene.h"
//#include "texture.h"
#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <emmintrin.h>
#include <fstream>
#include <iostream>
#include <random>
#include <stdlib.h>
#include <thread>
#include <utility>
#include <vector>

#include <clRNG/lfsr113.h>

static size_t toMultipleOf(size_t N, size_t base);
static int roundUp(int numToRound, int multiple);

template <typename T>
static void writeToBuffer(cl::CommandQueue& queue, cl::Buffer& buffer, gsl::span<T> items, size_t offset = 0);
template <typename T>
static void writeToBuffer(cl::CommandQueue& queue, cl::Buffer& buffer, gsl::span<T> items, size_t offset, std::vector<cl::Event>& events);

//#define OUTPUT_AVERAGE_GRAYSCALE
//#define RANDOM_XOR32
#define RANDOM_LFSR113

static constexpr uint32_t MAX_SAMPLES_PER_PIXEL = 20000000;
static constexpr uint32_t MAX_NUM_LIGHTS = 256;
static constexpr uint32_t MAX_ACTIVE_RAYS = 1280 * 720; // Number of rays per pass (top performance = all pixels in 1 pass but very large buffer sizes at high res)

struct KernelData {
    raytracer::CameraData camera;

    // Scene
    unsigned numEmissiveTriangles;
    unsigned topLevelBvhRoot;

    // Used for ray generation
    unsigned rayOffset;
    unsigned screenWidth;
    unsigned screenHeight;

    // Used for ray compaction
    unsigned numInRays;
    unsigned numOutRays;
    unsigned numShadowRays;
    unsigned maxRays;
    unsigned newRays;
};

namespace raytracer {
RayTracer::RayTracer(int width, int height, std::shared_ptr<Scene> scene, const UniqueTextureArray& materialTextures, const UniqueTextureArray& skydomeTextures, GLuint outputTarget)
    : m_samplesPerPixel(0)
    , m_screenWidth(width)
    , m_screenHeight(height)
    , m_clContext()
{

    m_generateRaysKernel = loadKernel("../../assets/cl/kernel.cl", "generatePrimaryRays");
    m_intersectShadowsKernel = loadKernel("../../assets/cl/kernel.cl", "intersectShadows");
    m_intersectWalkKernel = loadKernel("../../assets/cl/kernel.cl", "intersectWalk");
    m_shadingKernel = loadKernel("../../assets/cl/kernel.cl", "shade");
    m_updateKernelDataKernel = loadKernel("../../assets/cl/kernel.cl", "updateKernelData");
    m_accumulateKernel = loadKernel("../../assets/cl/accumulate.cl", "accumulate");

    m_topBvhRootNode[0] = 0;
    m_topBvhRootNode[1] = 0;

    m_numEmissiveTriangles[0] = 0;
    m_numEmissiveTriangles[1] = 0;

    setScene(scene, materialTextures);
    setSkydome(skydomeTextures);
    setTarget(outputTarget);
}

RayTracer::~RayTracer()
{
}

void RayTracer::rayTrace(const Camera& camera)
{
#ifdef OPENCL_GL_INTEROP
    // We must make sure that OpenGL is done with the textures, so we ask to sync.
    glFinish();

    std::vector<cl::Memory> images = { m_clGLInteropOutputImage };
    m_clContext.getGraphicsQueue().enqueueAcquireGLObjects(&images);
#endif

    // Easier than setting dirty flag all the time, bit more work intensive but not a big deal
    static CameraData prevFrameCamData = {};
    CameraData newCameraData = camera.get_camera_data();
    if (memcmp(&newCameraData, &prevFrameCamData, sizeof(CameraData)) != 0) {
        prevFrameCamData = newCameraData;
        clearAccumulationBuffer();
        m_samplesPerPixel = 0;
    }

    if (m_samplesPerPixel >= MAX_SAMPLES_PER_PIXEL)
        return;

    // Non blocking CPU
    traceRays(camera);
    accumulate(camera);
#ifdef OUTPUT_AVERAGE_GRAYSCALE
    CalculateAverageGrayscale();
#endif

    auto queue = m_clContext.getGraphicsQueue();
    queue.finish();

#ifndef OPENCL_GL_INTEROP
    // Copy OpenCL image to the CPU
    cl::size_t<3> o;
    o[0] = 0;
    o[1] = 0;
    o[2] = 0;
    cl::size_t<3> r;
    r[0] = m_screenWidth;
    r[1] = m_screenHeight;
    r[2] = 1;
    _queue.enqueueReadImage(m_clOutputImage, CL_TRUE, o, r, 0, 0, m_cpuOutputImage.get(), nullptr, nullptr);

    // And upload it to teh GPU (OpenGL)
    glBindTexture(GL_TEXTURE_2D, m_glOutputImage);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        m_screenWidth,
        m_screenHeight,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        m_cpuOutputImage.get());
#else
    // Before returning the objects to OpenGL, we sync to make sure OpenCL is done.
    cl_int err = queue.finish();
    checkClErr(err, "CommandQueue::finish");

    queue.enqueueReleaseGLObjects(&images);
#endif
}

void RayTracer::setSkydome(const UniqueTextureArray& skydomeTextureArray)
{
    m_skydomeTextures = std::make_unique<CLTextureArray>(skydomeTextureArray, m_clContext, 4000, 2000, true);
}

void RayTracer::frameTick()
{
    // Lot of CPU work
    copyNextAnimationFrameData();

    m_activeBuffer = (m_activeBuffer + 1) % 2;
}

int RayTracer::getSamplesPerPixel() const
{
    return m_samplesPerPixel;
}

int RayTracer::getMaxSamplesPerPixel() const
{
    return MAX_SAMPLES_PER_PIXEL;
}

void RayTracer::setScene(std::shared_ptr<Scene> scene, const UniqueTextureArray& textureArray)
{
    m_scene = scene;

    // Initialize buffers
    m_numStaticVertices = 0;
    m_numStaticTriangles = 0;
    m_numStaticMaterials = 0;
    m_numStaticBvhNodes = 0;

    uint32_t numVertices = 0;
    uint32_t numTriangles = 0;
    uint32_t numMaterials = 0;
    uint32_t numBvhNodes = 0;
    for (const auto& meshBvhPair : scene->getMeshes()) {
        auto meshID = meshBvhPair.meshID;

        if (meshID->isDynamic()) {
            numVertices += meshID->maxNumVertices();
            numTriangles += meshID->maxNumTriangles();
            numMaterials += meshID->maxNumMaterials();
            numBvhNodes += meshID->maxNumBvhNodes();
        } else {
            numVertices += (uint32_t)meshID->getVertices().size();
            numTriangles += (uint32_t)meshID->getTriangles().size();
            numMaterials += (uint32_t)meshID->getMaterials().size();
            numBvhNodes += (uint32_t)meshID->getBvhNodes().size();

            m_numStaticVertices += (uint32_t)meshID->getVertices().size();
            m_numStaticTriangles += (uint32_t)meshID->getTriangles().size();
            m_numStaticMaterials += (uint32_t)meshID->getMaterials().size();
            m_numStaticBvhNodes += (uint32_t)meshID->getBvhNodes().size();
        }
    }

    initBuffers(numVertices, numTriangles, MAX_NUM_LIGHTS, numMaterials,
        numBvhNodes, (uint32_t)scene->getMeshes().size() * 2, (uint32_t)scene->getLights().size());

    // Collect all static geometry and upload it to the GPU
    for (auto& meshBvhPair : scene->getMeshes()) {
        auto meshID = meshBvhPair.meshID;
        if (meshID->isDynamic())
            continue;

        // TODO: use memcpy instead of looping over vertices (faster?)
        uint32_t startVertex = (uint32_t)m_verticesHost.size();
        for (auto& vertex : meshID->getVertices()) {
            m_verticesHost.push_back(vertex);
        }

        uint32_t startMaterial = (uint32_t)m_materialsHost.size();
        for (auto& material : meshID->getMaterials()) {
            m_materialsHost.push_back(material);
        }

        uint32_t startTriangle = (uint32_t)m_trianglesHost.size();
        for (auto& triangle : meshID->getTriangles()) {
            m_trianglesHost.push_back(triangle);
            m_trianglesHost.back().indices += startVertex;
            m_trianglesHost.back().materialIndex += startMaterial;
        }

        uint32_t startBvhNode = (uint32_t)m_subBvhNodesHost.size();
        for (auto& bvhNode : meshID->getBvhNodes()) {
            m_subBvhNodesHost.push_back(bvhNode);
            auto& newNode = m_subBvhNodesHost.back();
            if (newNode.triangleCount > 0)
                newNode.firstTriangleIndex += startTriangle;
            else
                newNode.leftChildIndex += startBvhNode;
        }
        meshBvhPair.bvhOffset = startBvhNode;
    }

    auto queue = m_clContext.getGraphicsQueue();
    writeToBuffer(queue, m_verticesBuffers[0], gsl::make_span(m_verticesHost));
    writeToBuffer(queue, m_trianglesBuffers[0], gsl::make_span(m_trianglesHost));
    writeToBuffer(queue, m_materialsBuffers[0], gsl::make_span(m_materialsHost));
    writeToBuffer(queue, m_subBvhBuffers[0], gsl::make_span(m_subBvhNodesHost));

    writeToBuffer(queue, m_verticesBuffers[1], gsl::make_span(m_verticesHost));
    writeToBuffer(queue, m_trianglesBuffers[1], gsl::make_span(m_trianglesHost));
    writeToBuffer(queue, m_materialsBuffers[1], gsl::make_span(m_materialsHost));
    writeToBuffer(queue, m_subBvhBuffers[1], gsl::make_span(m_subBvhNodesHost));

    m_materialTextures = std::make_unique<CLTextureArray>(textureArray, m_clContext, 1024, 1024, false);

    frameTick();
}

void RayTracer::setTarget(GLuint glTexture)
{
    cl_int err;
#ifndef OPENCL_GL_INTEROP
    m_clOutputImage = cl::Image2D(_context,
        CL_MEM_WRITE_ONLY,
        cl::ImageFormat(CL_RGBA, CL_SNORM_INT8),
        m_screenWidth,
        m_screenHeight,
        0,
        0,
        &err);
    checkClErr(err, "Image2D");

    m_glOutputImage = glTexture;
    size_t size = m_screenWidth * m_screenHeight * 4;
    std::cout << "Output image cpu size: " << size << std::endl;
    float* mem = new float[size];
    m_cpuOutputImage = std::unique_ptr<float[]>(mem); // std::make_unique<float[]>(size);
#else
    m_clGLInteropOutputImage = cl::ImageGL(m_clContext, CL_MEM_WRITE_ONLY, GL_TEXTURE_2D, 0, glTexture, &err);
    checkClErr(err, "ImageGL");
#endif
}

void RayTracer::traceRays(const Camera& camera)
{
    auto queue = m_clContext.getGraphicsQueue();

    // Copy camera (and scene) data to the device using a struct so we dont use 20 kernel arguments
    KernelData data = {};

    data.camera = camera.get_camera_data();

    data.numEmissiveTriangles = m_numEmissiveTriangles[m_activeBuffer];
    data.topLevelBvhRoot = m_topBvhRootNode[m_activeBuffer];

    data.rayOffset = 0;
    data.screenWidth = (uint32_t)m_screenWidth;
    data.screenHeight = (uint32_t)m_screenHeight;

    data.numInRays = 0;
    data.numOutRays = 0;
    data.numShadowRays = 0;
    data.maxRays = MAX_ACTIVE_RAYS;
    data.newRays = 0;

    //for (int i = 0; i < 6; i++)
    //	data._skydomeTextureIndices[i] = _skydome_tex_indices[i];

    cl_int err = queue.enqueueWriteBuffer(
        m_kernelDataBuffer,
        CL_TRUE,
        0,
        sizeof(KernelData),
        &data);
    checkClErr(err, "CommandQueue::enqueueWriteBuffer");

    static_assert(MAX_ACTIVE_RAYS, "MAX_ACTIVE_RAYS must be a multiple of 64 (work group size)");
    int inRayBuffer = 0;
    int outRayBuffer = 1;
    uint32_t survivingRays = 0;
    while (true) {
        if (survivingRays != MAX_ACTIVE_RAYS) {
            // Generate primary rays and fill the emptyness
            m_generateRaysKernel.setArg(0, m_raysBuffer[inRayBuffer]);
            m_generateRaysKernel.setArg(1, m_kernelDataBuffer);
            m_generateRaysKernel.setArg(2, m_randomStreamBuffer);
            err = queue.enqueueNDRangeKernel(
                m_generateRaysKernel,
                cl::NullRange,
                cl::NDRange(roundUp(MAX_ACTIVE_RAYS - survivingRays, 32)),
                cl::NullRange); //cl::NDRange(64));
            checkClErr(err, "CommandQueue::enqueueNDRangeKernel()");
        }

        // Output data
        m_intersectWalkKernel.setArg(0, m_shadingRequestBuffer);
        // Input data
        m_intersectWalkKernel.setArg(1, m_raysBuffer[inRayBuffer]);
        m_intersectWalkKernel.setArg(2, m_rayTraversalBuffer);
        m_intersectWalkKernel.setArg(3, m_kernelDataBuffer);
        m_intersectWalkKernel.setArg(4, m_verticesBuffers[m_activeBuffer]);
        m_intersectWalkKernel.setArg(5, m_trianglesBuffers[m_activeBuffer]);
        m_intersectWalkKernel.setArg(6, m_subBvhBuffers[m_activeBuffer]);
        m_intersectWalkKernel.setArg(7, m_topBvhBuffers[m_activeBuffer]);
        m_intersectWalkKernel.setArg(8, m_accumulationBuffer);

        err = queue.enqueueNDRangeKernel(
            m_intersectWalkKernel,
            cl::NullRange,
            cl::NDRange(MAX_ACTIVE_RAYS),
            cl::NDRange(64));
        checkClErr(err, "CommandQueue::enqueueNDRangeKernel()");

        // Output data
        m_shadingKernel.setArg(0, m_accumulationBuffer);
        m_shadingKernel.setArg(1, m_raysBuffer[outRayBuffer]);
        m_shadingKernel.setArg(2, m_shadowRaysBuffer);
        // Input data
        m_shadingKernel.setArg(3, m_raysBuffer[inRayBuffer]);
        m_shadingKernel.setArg(4, m_shadingRequestBuffer);
        m_shadingKernel.setArg(5, m_kernelDataBuffer);
        // Static input data
        m_shadingKernel.setArg(6, m_verticesBuffers[m_activeBuffer]);
        m_shadingKernel.setArg(7, m_trianglesBuffers[m_activeBuffer]);
        m_shadingKernel.setArg(8, m_emissiveTrianglesBuffers[m_activeBuffer]);
        m_shadingKernel.setArg(9, m_materialsBuffers[m_activeBuffer]);
        m_shadingKernel.setArg(10, m_materialTextures->getImage2DArray());
        m_shadingKernel.setArg(11, m_skydomeTextures->getImage2DArray());
        m_shadingKernel.setArg(12, m_randomStreamBuffer);

        err = queue.enqueueNDRangeKernel(
            m_shadingKernel,
            cl::NullRange,
            cl::NDRange(MAX_ACTIVE_RAYS),
            cl::NDRange(64));
        checkClErr(err, "CommandQueue::enqueueNDRangeKernel()");

        // Request the output kernel data so we know the amount of surviving rays
        cl::Event updatedKernelDataEvent;
        KernelData updatedKernelData;
        err = queue.enqueueReadBuffer(
            m_kernelDataBuffer,
            CL_TRUE,
            0,
            sizeof(KernelData),
            &updatedKernelData);
        survivingRays = updatedKernelData.numOutRays;
        // Stop if we reach 0 rays and we processed the whole screen
        //updatedKernelDataEvent.wait();
        unsigned maxRays = m_screenWidth * m_screenHeight;
        if (survivingRays == 0 && // We are out of rays
            (updatedKernelData.rayOffset + updatedKernelData.newRays >= maxRays)) // And we wont generate new ones
            break;
        //survivingRays = MAX_ACTIVE_RAYS;

        if (survivingRays != 0) {
            m_intersectShadowsKernel.setArg(0, m_accumulationBuffer);
            m_intersectShadowsKernel.setArg(1, m_shadowRaysBuffer);
            m_intersectShadowsKernel.setArg(2, m_rayTraversalBuffer);
            m_intersectShadowsKernel.setArg(3, m_kernelDataBuffer);
            m_intersectShadowsKernel.setArg(4, m_verticesBuffers[m_activeBuffer]);
            m_intersectShadowsKernel.setArg(5, m_trianglesBuffers[m_activeBuffer]);
            m_intersectShadowsKernel.setArg(6, m_subBvhBuffers[m_activeBuffer]);
            m_intersectShadowsKernel.setArg(7, m_topBvhBuffers[m_activeBuffer]);

            err = queue.enqueueNDRangeKernel(
                m_intersectShadowsKernel,
                cl::NullRange,
                cl::NDRange(roundUp(survivingRays, 64)),
                cl::NDRange(64));
            checkClErr(err, "CommandQueue::enqueueNDRangeKernel()");
        }

        // Set num input rays to num output rays and set num out rays and num shadow rays to 0
        m_updateKernelDataKernel.setArg(0, m_kernelDataBuffer);
        err = queue.enqueueNDRangeKernel(
            m_updateKernelDataKernel,
            cl::NullRange,
            cl::NDRange(1),
            cl::NDRange(1));
        checkClErr(err, "CommandQueue::enqueueNDRangeKernel()");

        // What used to be output is now the input to the pass
        std::swap(inRayBuffer, outRayBuffer);
    }

    m_samplesPerPixel += 1;
}

void RayTracer::accumulate(const Camera& camera)
{
#ifdef OPENCL_GL_INTEROP
    m_accumulateKernel.setArg(0, m_clGLInteropOutputImage);
#else
    m_accumulateKernel.setArg(0, m_clOutputImage);
#endif
    m_accumulateKernel.setArg(1, m_accumulationBuffer);
    m_accumulateKernel.setArg(2, m_kernelDataBuffer);
    m_accumulateKernel.setArg(3, m_samplesPerPixel);
    m_accumulateKernel.setArg(4, m_screenWidth);
    m_clContext.getGraphicsQueue().enqueueNDRangeKernel(
        m_accumulateKernel,
        cl::NullRange,
        cl::NDRange(m_screenWidth, m_screenHeight),
        cl::NullRange,
        nullptr,
        nullptr);
}

void RayTracer::clearAccumulationBuffer()
{
    cl_float3 zero = {};
    m_clContext.getGraphicsQueue().enqueueFillBuffer(
        m_accumulationBuffer,
        zero,
        0,
        m_screenWidth * m_screenHeight * sizeof(cl_float3),
        nullptr,
        nullptr);
}

void RayTracer::calculateAverageGrayscale()
{
    size_t sizeInVecs = m_screenWidth * m_screenHeight;
    auto buffer = std::make_unique<glm::vec4[]>(sizeInVecs);
    m_clContext.getGraphicsQueue().enqueueReadBuffer(
        m_accumulationBuffer,
        CL_TRUE,
        0,
        sizeInVecs * sizeof(cl_float3),
        buffer.get(),
        nullptr,
        nullptr);

    float sumLeft = 0.0f;
    float sumRight = 0.0f;
    for (size_t i = 0; i < sizeInVecs; i++) {
        // https://en.wikipedia.org/wiki/Grayscale
        glm::vec3 colour = glm::vec3(buffer[i]) / (float)m_samplesPerPixel;
        float grayscale = 0.2126f * colour.r + 0.7152f * colour.g + 0.0722f * colour.b;

        auto col = i % m_screenWidth;
        if (col < m_screenWidth / 2)
            sumLeft += grayscale;
        else
            sumRight += grayscale;
    }
    sumLeft /= (float)sizeInVecs * 2;
    sumRight /= (float)sizeInVecs * 2;
    std::cout << "Average grayscale value left:  " << sumLeft << std::endl;
    std::cout << "Average grayscale value right: " << sumRight << "\n"
              << std::endl;
}

void RayTracer::copyNextAnimationFrameData()
{
    auto graphicsQueue = m_clContext.getGraphicsQueue();
    auto copyQueue = m_clContext.getCopyQueue();

    // Manually flush the queue
    // At least on AMD, the queue is flushed after the enqueueWriteBuffer (probably because it thinks
    //  one kernel launch is not enough reason to flush). So it would be executed at _queue.finish(), which
    //  is called after the top lvl bvh construction (which is expensive) has completed. Instead we manually
    //  flush and than calculate the top lvl bvh.
    graphicsQueue.flush();

    int copyBuffers = (m_activeBuffer + 1) % 2;
    std::vector<cl::Event> waitEvents;

    m_verticesHost.resize(m_numStaticVertices);
    m_trianglesHost.resize(m_numStaticTriangles);
    m_materialsHost.resize(m_numStaticMaterials);
    m_subBvhNodesHost.resize(m_numStaticBvhNodes);

    // Collect all static geometry and upload it to the GPU
    for (auto& meshBvhPair : m_scene->getMeshes()) {
        auto meshID = meshBvhPair.meshID;
        if (!meshID->isDynamic())
            continue;

        meshID->buildBvh();

        // TODO: use memcpy instead of looping over vertices (faster?)
        uint32_t startVertex = (uint32_t)m_verticesHost.size();
        for (const auto& vertex : meshID->getVertices()) {
            m_verticesHost.push_back(vertex);
        }

        uint32_t startMaterial = (uint32_t)m_materialsHost.size();
        for (const auto& material : meshID->getMaterials()) {
            m_materialsHost.push_back(material);
        }

        uint32_t startTriangle = (uint32_t)m_trianglesHost.size();
        for (const auto& triangle : meshID->getTriangles()) {
            m_trianglesHost.push_back(triangle);
            m_trianglesHost.back().indices += startVertex;
            m_trianglesHost.back().materialIndex += startMaterial;
        }

        uint32_t startBvhNode = (uint32_t)m_subBvhNodesHost.size();
        for (const auto& bvhNode : meshID->getBvhNodes()) {
            m_subBvhNodesHost.push_back(bvhNode);
            auto& newNode = m_subBvhNodesHost.back();
            if (newNode.triangleCount > 0)
                newNode.firstTriangleIndex += startTriangle;
            else
                newNode.leftChildIndex += startBvhNode;
        }
        meshBvhPair.bvhOffset = startBvhNode;
    }

    // Get the light emmiting triangles transformed by the scene graph
    m_emissiveTrianglesHost.clear();
    collectTransformedLights(&m_scene->getRootNode(), glm::mat4());
    m_numEmissiveTriangles[copyBuffers] = (uint32_t)m_emissiveTrianglesHost.size();
    writeToBuffer(copyQueue, m_emissiveTrianglesBuffers[copyBuffers], gsl::make_span(m_emissiveTrianglesHost), 0, waitEvents);

    if (m_verticesHost.size() > static_cast<size_t>(m_numStaticVertices)) // Dont copy if we dont have any dynamic geometry
    {
        writeToBuffer(copyQueue, m_verticesBuffers[copyBuffers], gsl::make_span(m_verticesHost), m_numStaticVertices, waitEvents);
        writeToBuffer(copyQueue, m_trianglesBuffers[copyBuffers], gsl::make_span(m_trianglesHost), m_numStaticTriangles, waitEvents);
        writeToBuffer(copyQueue, m_materialsBuffers[copyBuffers], gsl::make_span(m_materialsHost), m_numStaticMaterials, waitEvents);
        writeToBuffer(copyQueue, m_subBvhBuffers[copyBuffers], gsl::make_span(m_subBvhNodesHost), m_numStaticBvhNodes, waitEvents);
    }

    // Update the top level BVH and copy it to the GPU on a seperate copy queue
    m_topBvhNodesHost.clear();
    auto bvhBuilder = TopLevelBvhBuilder(*m_scene.get());
    m_topBvhRootNode[copyBuffers] = bvhBuilder.build(m_subBvhNodesHost, m_topBvhNodesHost);
    writeToBuffer(copyQueue, m_topBvhBuffers[copyBuffers], gsl::make_span(m_topBvhNodesHost), 0, waitEvents);

    if (m_verticesHost.size() > static_cast<size_t>(m_numStaticVertices)) {
        timeOpenCL(waitEvents[0], "vertex upload");
        timeOpenCL(waitEvents[1], "triangle upload");
        timeOpenCL(waitEvents[2], "material upload");
        timeOpenCL(waitEvents[3], "sub bvh upload");
        timeOpenCL(waitEvents[4], "emissive triangles upload");
        timeOpenCL(waitEvents[5], "top bvh upload");
    } else {
        //timeOpenCL(waitEvents[0], "emissive triangles upload");
        //timeOpenCL(waitEvents[1], "top bvh upload");
    }

    // Make sure the main queue waits for the copy to finish
    cl_int err = graphicsQueue.enqueueBarrierWithWaitList(&waitEvents);
    checkClErr(err, "CommandQueue::enqueueBarrierWithWaitList");
}

void RayTracer::collectTransformedLights(const SceneNode* node, const glm::mat4& transform)
{
    auto newTransform = transform * node->transform.matrix();
    if (node->meshID != -1) {
        auto& meshID = m_scene->getMeshes()[node->meshID];
        auto& vertices = meshID.meshID->getVertices();
        auto& triangles = meshID.meshID->getTriangles();
        auto& emissiveTriangles = meshID.meshID->getEmissiveTriangles();
        auto& materials = meshID.meshID->getMaterials();

        for (auto& triangleIndex : emissiveTriangles) {
            auto& triangle = triangles[triangleIndex];
            EmissiveTriangle result;
            result.vertices[0] = newTransform * vertices[triangle.indices[0]].vertex;
            result.vertices[1] = newTransform * vertices[triangle.indices[1]].vertex;
            result.vertices[2] = newTransform * vertices[triangle.indices[2]].vertex;
            result.material = materials[triangle.materialIndex];
            m_emissiveTrianglesHost.push_back(result);
        }
    }

    for (auto& child : node->children) {
        collectTransformedLights(child.get(), newTransform);
    }
}

void RayTracer::initBuffers(
    uint32_t numVertices,
    uint32_t numTriangles,
    uint32_t numEmissiveTriangles,
    uint32_t numMaterials,
    uint32_t numSubBvhNodes,
    uint32_t numTopBvhNodes,
    uint32_t numLights)
{
    cl_int err;

    m_verticesBuffers[0] = cl::Buffer(m_clContext,
        CL_MEM_READ_ONLY,
        std::max(1u, numVertices) * sizeof(VertexSceneData),
        NULL,
        &err);
    m_verticesBuffers[1] = cl::Buffer(m_clContext,
        CL_MEM_READ_ONLY,
        std::max(1u, numVertices) * sizeof(VertexSceneData),
        NULL,
        &err);
    checkClErr(err, "Buffer::Buffer()");

    m_trianglesBuffers[0] = cl::Buffer(m_clContext,
        CL_MEM_READ_ONLY,
        std::max(1u, numTriangles) * sizeof(TriangleSceneData),
        NULL,
        &err);
    m_trianglesBuffers[1] = cl::Buffer(m_clContext,
        CL_MEM_READ_ONLY,
        std::max(1u, numTriangles) * sizeof(TriangleSceneData),
        NULL,
        &err);
    checkClErr(err, "Buffer::Buffer()");

    m_emissiveTrianglesBuffers[0] = cl::Buffer(m_clContext,
        CL_MEM_READ_ONLY,
        std::max(1u, numEmissiveTriangles) * sizeof(EmissiveTriangle),
        NULL,
        &err);
    m_emissiveTrianglesBuffers[1] = cl::Buffer(m_clContext,
        CL_MEM_READ_ONLY,
        std::max(1u, numEmissiveTriangles) * sizeof(EmissiveTriangle),
        NULL,
        &err);
    checkClErr(err, "Buffer::Buffer()");

    m_materialsBuffers[0] = cl::Buffer(m_clContext,
        CL_MEM_READ_ONLY,
        std::max(1u, numMaterials) * sizeof(Material),
        NULL,
        &err);
    m_materialsBuffers[1] = cl::Buffer(m_clContext,
        CL_MEM_READ_ONLY,
        std::max(1u, numMaterials) * sizeof(Material),
        NULL,
        &err);
    checkClErr(err, "Buffer::Buffer()");

    m_subBvhBuffers[0] = cl::Buffer(m_clContext,
        CL_MEM_READ_ONLY,
        std::max(1u, numSubBvhNodes) * sizeof(SubBvhNode),
        NULL,
        &err);
    m_subBvhBuffers[1] = cl::Buffer(m_clContext,
        CL_MEM_READ_ONLY,
        std::max(1u, numSubBvhNodes) * sizeof(SubBvhNode),
        NULL,
        &err);
    checkClErr(err, "Buffer::Buffer()");

    m_topBvhBuffers[0] = cl::Buffer(m_clContext,
        CL_MEM_READ_ONLY,
        numTopBvhNodes * sizeof(TopBvhNode), // TODO: Make this dynamic so we dont have a fixed max of 256 top level BVH nodes.
        NULL,
        &err);
    checkClErr(err, "Buffer::Buffer()");
    m_topBvhBuffers[1] = cl::Buffer(m_clContext,
        CL_MEM_READ_ONLY,
        numTopBvhNodes * sizeof(TopBvhNode), // TODO: Make this dynamic so we dont have a fixed max of 256 top level BVH nodes.
        NULL,
        &err);
    checkClErr(err, "Buffer::Buffer()");

    m_rayTraversalBuffer = cl::Buffer(m_clContext,
        CL_MEM_READ_WRITE,
        MAX_ACTIVE_RAYS * 32 * sizeof(uint32_t),
        NULL,
        &err);
    checkClErr(err, "Buffer::Buffer()");

    m_kernelDataBuffer = cl::Buffer(m_clContext,
        CL_MEM_READ_WRITE,
        sizeof(KernelData),
        NULL,
        &err);
    checkClErr(err, "Buffer::Buffer()");

    // Create random streams and copy them to the GPU
    size_t numWorkItems = m_screenWidth * m_screenHeight;
#ifdef RANDOM_XOR32
    size_t streamBufferSize = numWorkItems * sizeof(uint32_t);
    auto streams = std::make_unique<uint32_t[]>(numWorkItems);

    // Generate random unsigneds the C++11 way
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis;
    for (size_t i = 0; i < numWorkItems; i++)
        streams[i] = dis(gen);

    m_randomStreamBuffer = cl::Buffer(_context,
        CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
        streamBufferSize,
        streams.get(),
        &err);
    checkClErr(err, "Buffer::Buffer()");
#elif defined(RANDOM_LFSR113)
    size_t streamBufferSize;
    clrngLfsr113Stream* streams = clrngLfsr113CreateStreams(
        NULL, numWorkItems, &streamBufferSize, (clrngStatus*)&err);
    checkClErr(err, "clrngLfsr113CreateStreams");
    m_randomStreamBuffer = cl::Buffer(m_clContext,
        CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
        streamBufferSize,
        streams,
        &err);
    checkClErr(err, "Buffer::Buffer()");
    clrngLfsr113DestroyStreams(streams);
#endif

    m_accumulationBuffer = cl::Buffer(m_clContext,
        CL_MEM_READ_WRITE,
        m_screenWidth * m_screenHeight * sizeof(cl_float3),
        nullptr,
        &err);
    checkClErr(err, "cl::Buffer");

    const int rayDataStructSize = 80;
    m_raysBuffer[0] = cl::Buffer(m_clContext,
        CL_MEM_READ_WRITE,
        (size_t)MAX_ACTIVE_RAYS * rayDataStructSize,
        nullptr,
        &err);
    checkClErr(err, "cl::Buffer");
    m_raysBuffer[1] = cl::Buffer(m_clContext,
        CL_MEM_READ_WRITE,
        (size_t)MAX_ACTIVE_RAYS * rayDataStructSize,
        nullptr,
        &err);
    checkClErr(err, "cl::Buffer");

    m_shadowRaysBuffer = cl::Buffer(m_clContext,
        CL_MEM_READ_WRITE,
        (size_t)MAX_ACTIVE_RAYS * rayDataStructSize,
        nullptr,
        &err);
    checkClErr(err, "cl::Buffer");

    const int shadingDataStructSize = 64;
    m_shadingRequestBuffer = cl::Buffer(m_clContext,
        CL_MEM_READ_WRITE,
        (size_t)MAX_ACTIVE_RAYS * shadingDataStructSize,
        nullptr,
        &err);
    checkClErr(err, "cl::Buffer");
}

cl::Kernel RayTracer::loadKernel(std::string_view fileName, std::string_view funcName)
{
    cl_int err;

    std::ifstream file(fileName.data());
    {
        std::string errorMessage = "Cannot open file: ";
        errorMessage += fileName;
        checkClErr(file.is_open() ? CL_SUCCESS : -1, errorMessage.c_str());
    }

    std::vector<cl::Device> devices;
    devices.push_back(m_clContext.getDevice());

    std::string prog(std::istreambuf_iterator<char>(file),
        (std::istreambuf_iterator<char>()));
    cl::Program::Sources sources;
    sources.push_back(std::make_pair(prog.c_str(), prog.length()));
    cl::Program program(m_clContext, sources);
    std::string opts = "-I ../../assets/cl/ -I ../../assets/cl/clRNG/ ";
#ifdef RANDOM_XOR32
    opts += "-D RANDOM_XOR32 ";
#elif defined(RANDOM_LFSR113)
    opts += "-D RANDOM_LFSR113 ";
#endif

#if defined(_DEBUG)
    //opts += "-cl-std=CL1.2 -g -O0"; // -g is not supported on all compilers. If you have problems, remove this option
#else
    opts += "-cl-mad-enable -cl-unsafe-math-optimizations -cl-finite-math-only -cl-fast-relaxed-math -cl-single-precision-constant";
#endif

    err = program.build(devices, opts.c_str());
    {
        if (err != CL_SUCCESS) {
            std::string errorMessage = "Cannot build program: ";
            errorMessage += fileName;
            std::cout << "Cannot build program: " << fileName << std::endl;

            std::string error;
            program.getBuildInfo(m_clContext.getDevice(), CL_PROGRAM_BUILD_LOG, &error);
            std::cout << error << std::endl;

#ifdef _WIN32
            system("PAUSE");
#endif
            exit(EXIT_FAILURE);
        }
    }

    cl::Kernel kernel(program, funcName.data(), &err);
    {
        std::string errorMessage = "Cannot create kernel: ";
        errorMessage += fileName;
        checkClErr(err, errorMessage.c_str());
    }

    return kernel;
}
}

static size_t toMultipleOf(size_t N, size_t base)
{
    return static_cast<size_t>((ceil((double)N / (double)base) * base));
}

// http://stackoverflow.com/questions/3407012/c-rounding-up-to-the-nearest-multiple-of-a-number
static int roundUp(int numToRound, int multiple)
{
    if (multiple == 0)
        return numToRound;

    int remainder = numToRound % multiple;
    if (remainder == 0)
        return numToRound;

    return numToRound + multiple - remainder;
}

template <typename T>
static void writeToBuffer(
    cl::CommandQueue& queue,
    cl::Buffer& buffer,
    gsl::span<T> items,
    size_t offset)
{
    if (items.size() == 0)
        return;

    cl_int err = queue.enqueueWriteBuffer(
        buffer,
        CL_TRUE,
        offset * sizeof(T),
        (items.size() - offset) * sizeof(T),
        &items[offset]);
    checkClErr(err, "CommandQueue::enqueueWriteBuffer");
}

template <typename T>
static void writeToBuffer(
    cl::CommandQueue& queue,
    cl::Buffer& buffer,
    gsl::span<T> items,
    size_t offset,
    std::vector<cl::Event>& events)
{
    if (items.size() == 0)
        return;

    cl::Event ev;
    cl_int err = queue.enqueueWriteBuffer(
        buffer,
        CL_TRUE,
        offset * sizeof(T),
        (items.size() - offset) * sizeof(T),
        &items[offset],
        nullptr,
        &ev);
    events.push_back(ev);
    checkClErr(err, "CommandQueue::enqueueWriteBuffer");
}
