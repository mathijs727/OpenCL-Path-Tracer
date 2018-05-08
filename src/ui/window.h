#pragma once
#include "opencl/cl_gl_includes.h"
#include <GLFW/glfw3.h>
#include <functional>
#include <glm/vec2.hpp>
#include <string_view>
#include <vector>

namespace ui {
class Window {
public:
    Window(uint32_t width, uint32_t height, std::string_view title);
    ~Window();

    void processInput();
    void swapBuffers();
    bool shouldClose() const; // Whether the user clicked on the close button

    using KeyCallback = std::function<void(int key, int scancode, int action, int mods)>;
    void registerKeyCallback(KeyCallback&&);

    using MouseButtonCallback = std::function<void(int button, int action, int mods)>;
    void registerMouseButtonCallback(MouseButtonCallback&&);

    using MouseMoveCallback = std::function<void(glm::dvec2 newPosition)>;
    void registerMouseMoveCallback(MouseMoveCallback&&);

    bool isKeyDown(int key) const;

    void setMouseCapture(bool capture);

    uint32_t getWidth() const;
    uint32_t getHeight() const;

private:
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void mouseMoveCallback(GLFWwindow* window, double xpos, double ypos);

private:
    uint32_t m_width, m_height;
    GLFWwindow* m_window;

    std::vector<KeyCallback> m_keyCallbacks;
    std::vector<MouseButtonCallback> m_mouseButtonCallbacks;
    std::vector<MouseMoveCallback> m_mouseMoveCallbacks;
};
}
