#include "bvh/bvh_test.h"
#include "camera.h"
#include "common.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "opencl/texture.h"
#include "raytracer.h"
#include "scene.h"
#include "timer.h"
#include "transform.h"
#include "ui/gloutput.h"
#include "ui/window.h"
#include <glm/glm.hpp>
#include <string_view>
#include <filesystem>

const std::filesystem::path basePath = BASE_PATH;

using namespace ui;
using namespace raytracer;

static const uint32_t screenWidth = 1280;
static const uint32_t screenHeight = 720;
static const double cameraViewSpeed = 0.05;
static const double cameraMoveSpeed = 1.0;

void createScene(Scene& scene, UniqueTextureArray& textureArray);
void createSkydome(const std::filesystem::path& filePath, bool isLinear, float brightnessMultiplier, UniqueTextureArray& textureArray);

void cameraLookHandler(Camera& camera, glm::dvec2 mousePosition, bool ignoreMovement);
void cameraMoveHandler(Camera& camera, const ui::Window& window, double dt);

int main(int argc, char* argv[])
{
#if 0 // Test BVH build only
    auto scene = std::make_shared<Scene>();
    UniqueTextureArray materialTextures;
    createScene(*scene, materialTextures);

    system("PAUSE");
#else
    glm::vec3 cameraEuler = glm::vec3(0.0f, Pi<float>::value, 0.0f);

    Transform cameraTransform;
    cameraTransform.location = glm::vec3(-0.540209413f, 0.893056393f, 0.681102335f);
    cameraTransform.orientation = glm::quat(0.206244200f, 0.0533384047f, 0.945924520f, -0.244632825f); // identity
    Camera camera(cameraTransform, 100.0f, (float)screenWidth / screenHeight, 1.0f);
    camera.m_thinLens = false;

    auto scene = std::make_shared<Scene>();
    UniqueTextureArray materialTextures;
    createScene(*scene, materialTextures);
    UniqueTextureArray skydomeTextures;
    createSkydome(basePath  / "assets/skydome/DF360_005_Ref.hdr", true, 75.0f, skydomeTextures);

    // Window creates OpenGL context so all OpenGL code should come afterwards.
    ui::Window window(screenWidth, screenHeight, "Hello world!");
    GLOutput output(screenWidth, screenHeight);

    bool mouseCaptured = false;
    window.setMouseCapture(mouseCaptured);
    window.registerMouseMoveCallback([&](glm::dvec2 mousePosition) {
        cameraLookHandler(camera, mousePosition, !mouseCaptured);
    });

    bool userClose = false;
    window.registerKeyCallback([&](int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_LEFT_ALT && action == GLFW_PRESS) {
            mouseCaptured = !mouseCaptured;
            window.setMouseCapture(mouseCaptured);
        }

        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
            userClose = true;
        }
    });

    RayTracer rayTracer(screenWidth, screenHeight, scene, materialTextures, skydomeTextures, output.getGLTexture());

    auto updateUI = [&]() {
        // Taken from the example code
        // 1. Show a simple window
        // Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets appears in a window automatically called "Debug"
        ImGui::Begin("Camera Widget");

        //ImGui::Text("Hello, world!");
        ImGui::Checkbox("Thin lens", &camera.m_thinLens);
        if (camera.m_thinLens) {
            ImGui::SliderFloat("Focal distance (m)", &camera.m_focalDistance, 0.1f, 5.0f);
            ImGui::SliderFloat("Focal length (mm)", &camera.m_focalLengthMm, 10.0f, 100.0f);
        }
        ImGui::SliderFloat("Aperture (f-stops)", &camera.m_aperture, 1.0f, 10.0f);
        ImGui::SliderFloat("Shutter time (s)", &camera.m_shutterTime, 1.0f / 500.0f, 1.0f / 32.0f);
        ImGui::SliderFloat("Sensitivity (ISO)", &camera.m_iso, 100.0f, 3200.0f);

        ImGui::Separator();

        ImGui::Text("%d / %d samples per pixel", rayTracer.getSamplesPerPixel(), rayTracer.getMaxSamplesPerPixel());
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

        ImGui::End();
    };

    Timer timer;
    while (!window.shouldClose() && !userClose) {
        auto now = std::chrono::high_resolution_clock::now();
        double dt = timer.elapsed<double>();
        timer.reset();

        window.processInput();
        cameraMoveHandler(camera, window, dt);

        rayTracer.rayTrace(camera);
        output.render();

        updateUI();
        window.swapBuffers();
    }
#endif
}

void createScene(Scene& scene, UniqueTextureArray& textureArray)
{
    // Light plane
    {
        Transform transform;
        transform.location = glm::vec3(0, 10, -0.5f);
        transform.scale = glm::vec3(20, 1, 10);
        transform.orientation = glm::quat(glm::vec3(Pi<float>::value, 0, 0)); // Flip upside down
        auto lightPlane = std::make_shared<Mesh>(basePath / "assets/3dmodels/plane/plane.obj", Material::Emissive(5500.0f, 1000.0f), textureArray);
        scene.addNode(lightPlane, transform);
    }

    // Sponza
    {
        Transform transform;
        transform.scale = glm::vec3(0.005f);
        auto sponza = std::make_shared<Mesh>(basePath  / "assets/3dmodels/sponza-crytek/sponza.obj", textureArray);
        scene.addNode(sponza, transform);
    }

    // Stanford bunny
    {
        Transform transform;
        transform.scale = glm::vec3(4.0f);
        auto bunny = std::make_shared<Mesh>(
            basePath  / "assets/3dmodels/stanford/bunny/bun_zipper.ply",
            Material::PBRMetal(
                glm::vec3(0.955f, 0.638f, 0.538f), // Copper
                0.8f),
            textureArray);
        BvhTester bvhTest(bunny);
        bvhTest.test();
        scene.addNode(bunny, transform);
    }
}

void createSkydome(const std::filesystem::path& filePath, bool isLinear, float brightnessMultiplier, UniqueTextureArray& textureArray)
{
    textureArray.add(filePath, isLinear, brightnessMultiplier);
}

void cameraLookHandler(Camera& camera, glm::dvec2 mousePosition, bool ignoreMovement)
{
    static glm::vec3 cameraEuler = glm::vec3(0.0f, Pi<float>::value, 0.0f);
    static glm::dvec2 prevMousePosition = glm::dvec2(0.0f);
    if (static bool firstFrame = true; firstFrame) {
        prevMousePosition = mousePosition;
        firstFrame = false;
    }

    if (!ignoreMovement) {
        glm::dvec2 delta = mousePosition - prevMousePosition;
        cameraEuler.x += glm::radians(static_cast<float>(delta.y * cameraViewSpeed));
        cameraEuler.y += glm::radians(static_cast<float>(delta.x * cameraViewSpeed));
        cameraEuler.x = glm::clamp(cameraEuler.x, -glm::half_pi<float>(), glm::half_pi<float>());

        if (delta.x != 0.0 || delta.y != 0.0) {
            Transform transform = camera.getTransform();
            transform.orientation = glm::quat(cameraEuler);
            camera.setTransform(transform);
        }
    }

    prevMousePosition = mousePosition;
}

void cameraMoveHandler(Camera& camera, const ui::Window& window, double dt)
{

    glm::vec3 movement = glm::vec3(0);
    movement.z += window.isKeyDown(GLFW_KEY_W) ? 1 : 0;
    movement.z -= window.isKeyDown(GLFW_KEY_S) ? 1 : 0;
    movement.x += window.isKeyDown(GLFW_KEY_D) ? 1 : 0;
    movement.x -= window.isKeyDown(GLFW_KEY_A) ? 1 : 0;
    movement.y += window.isKeyDown(GLFW_KEY_SPACE) ? 1 : 0;
    movement.y -= window.isKeyDown(GLFW_KEY_C) ? 1 : 0;

    if (movement != glm::vec3(0)) {
        movement *= static_cast<float>(dt * cameraMoveSpeed);
        Transform transform = camera.getTransform();
        transform.location += glm::mat3_cast(transform.orientation) * movement;
        camera.setTransform(transform);
    }
}
