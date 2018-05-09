#include "camera.h"
#include "common.h"
#include "raytracer.h"
#include "scene.h"
#include "texture.h"
#include "timer.h"
#include "transform.h"
#include "ui/gloutput.h"
#include "window.h"
#include <glm/glm.hpp>

using namespace ui;
using namespace raytracer;

static const uint32_t screenWidth = 1280;
static const uint32_t screenHeight = 720;
static const double cameraViewSpeed = 0.1;
static const double cameraMoveSpeed = 1.0f;

void createScene(Scene& scene, UniqueTextureArray& textureArray);
void createSkydome(std::string_view fileName, bool isLinear, float brightnessMultiplier, UniqueTextureArray& textureArray);

void cameraLookHandler(Camera& camera, glm::dvec2 mousePosition);
void cameraMoveHandler(Camera& camera, const Window& window, double dt);

int main(int argc, char* argv[])
{
    glm::vec3 cameraEuler = glm::vec3(0.0f, Pi<float>::value, 0.0f);

    Transform cameraTransform;
    cameraTransform.location = glm::vec3(2.5f, 2.0f, 0.01f);
    //cameraTransform.orientation = glm::quat(0.803762913, -0.128022775, -0.573779523, -0.0913911909); // identity
    Camera camera(cameraTransform, 100.0f, (float)screenWidth / screenHeight, 1.0f);
    camera.m_thinLens = false;

    auto scene = std::make_shared<Scene>();
    UniqueTextureArray materialTextures;
    createScene(*scene, materialTextures);
    UniqueTextureArray skydomeTextures;
    createSkydome("../../assets/skydome/DF360_005_Ref.hdr", true, 75.0f, skydomeTextures);

    // Window creates OpenGL context so all OpenGL code should come afterwards.
    Window window(screenWidth, screenHeight, "Hello world!");
    GLOutput output(screenWidth, screenHeight);

    bool mouseCaptured = true;
    window.setMouseCapture(mouseCaptured);
    window.registerMouseMoveCallback([&](glm::dvec2 mousePosition) {
        if (mouseCaptured) {
            cameraLookHandler(camera, mousePosition);
        }
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

    Timer timer;
    while (!window.shouldClose() && !userClose) {
        auto now = std::chrono::high_resolution_clock::now();
        double dt = timer.elapsed<double>();
        timer.reset();

        window.processInput();
        cameraMoveHandler(camera, window, dt);

        rayTracer.rayTrace(camera);
        output.render();

        window.swapBuffers();
    }
}

void createScene(Scene& scene, UniqueTextureArray& textureArray)
{
    // Light plane
    {
        Transform transform;
        transform.location = glm::vec3(0, 10, -0.5f);
        transform.scale = glm::vec3(20, 1, 10);
        transform.orientation = glm::quat(glm::vec3(Pi<float>::value, 0, 0)); // Flip upside down
        auto lightPlane = std::make_shared<Mesh>("../../assets/3dmodels/plane/plane.obj", Material::Emissive(5500.0f, 1000.0f), textureArray);
        scene.addNode(lightPlane, transform);
    }

    // Sponza
    {
        Transform transform;
        transform.scale = glm::vec3(0.005f);
        auto sponza = std::make_shared<Mesh>("../../assets/3dmodels/sponza-crytek/sponza.obj", textureArray);
        scene.addNode(sponza, transform);
    }

    // Stanford bunny
    {
        Transform transform;
        transform.scale = glm::vec3(4.0f);
        auto bunny = std::make_shared<Mesh>(
            "../../assets/3dmodels/stanford/bunny/bun_zipper.ply",
            Material::PBRMetal(
                glm::vec3(0.955f, 0.638f, 0.538f), // Copper
                0.8f),
            textureArray);
        scene.addNode(bunny, transform);
    }
}

void createSkydome(std::string_view fileName, bool isLinear, float brightnessMultiplier, UniqueTextureArray& textureArray)
{
    textureArray.add(fileName, isLinear, brightnessMultiplier);
}

void cameraLookHandler(Camera& camera, glm::dvec2 mousePosition)
{
    static glm::vec3 cameraEuler = glm::vec3(0.0f, Pi<float>::value, 0.0f);
    static glm::dvec2 prevMousePosition = glm::dvec2(0.0f);
    if (static bool firstFrame = true; firstFrame) {
        prevMousePosition = mousePosition;
        firstFrame = false;
    }

    glm::dvec2 delta = mousePosition - prevMousePosition;
    prevMousePosition = mousePosition;

    cameraEuler.x += glm::radians(static_cast<float>(delta.y * cameraViewSpeed));
    cameraEuler.y += glm::radians(static_cast<float>(delta.x * cameraViewSpeed));
    cameraEuler.x = glm::clamp(cameraEuler.x, -glm::half_pi<float>(), glm::half_pi<float>());

    if (delta.x != 0.0 || delta.y != 0.0) {
        Transform transform = camera.getTransform();
        transform.orientation = glm::quat(cameraEuler);
        camera.setTransform(transform);
    }
}

void cameraMoveHandler(Camera& camera, const Window& window, double dt)
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
