#include "window.h"
#include <iostream>
#include <string>
#include <GL/glew.h>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

static void fail(std::string_view errorMessage)
{
    std::cerr << errorMessage << std::endl;
    throw std::runtime_error(errorMessage.data());
}

void errorCallback(int error, const char* description)
{
    std::string errorMessage = "GLFW error: ";
    errorMessage += description;
    fail(errorMessage);
}

namespace ui {

Window::Window(uint32_t width, uint32_t height, std::string_view title)
    : m_width(width)
    , m_height(height)
{
    if (!glfwInit()) {
        fail("Could not initialize GLFW");
    }
    glfwSetErrorCallback(errorCallback);

    m_window = glfwCreateWindow(width, height, title.data(), nullptr, nullptr);
    if (m_window == nullptr) {
        glfwTerminate();
        fail("Could not create GLFW window");
    }
    glfwMakeContextCurrent(m_window);

    if (glewInit() == GLEW_ERROR_NO_GL_VERSION) {
        glfwTerminate();
        fail("Could not initialize GLEW");
    }

    glfwSetWindowUserPointer(m_window, this);
    glfwSetKeyCallback(m_window, keyCallback);
    glfwSetMouseButtonCallback(m_window, mouseButtonCallback);
    glfwSetCursorPosCallback(m_window, mouseMoveCallback);

	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	(void)io;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

	ImGui_ImplGlfw_InitForOpenGL(m_window, false);
	ImGui_ImplOpenGL3_Init("#version 150");

	// Start first ImGui frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	// Style
	ImGui::StyleColorsDark();
}

Window::~Window()
{
	// Cleanup
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

    glfwDestroyWindow(m_window);
    glfwTerminate();
}

void Window::processInput()
{
    glfwPollEvents();
}

void Window::swapBuffers()
{
	// ImGui rendering
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(m_window);

	// Start new ImGui frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

bool Window::shouldClose() const
{
    return glfwWindowShouldClose(m_window) != 0;
}

void Window::registerKeyCallback(KeyCallback&& callback)
{
    m_keyCallbacks.push_back(std::move(callback));
}

void Window::registerMouseButtonCallback(MouseButtonCallback&& callback)
{
    m_mouseButtonCallbacks.push_back(std::move(callback));
}

void Window::registerMouseMoveCallback(MouseMoveCallback&& callback)
{
    m_mouseMoveCallbacks.push_back(std::move(callback));
}

void Window::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    Window* thisWindow = static_cast<Window*>(glfwGetWindowUserPointer(window));

    for (auto& callback : thisWindow->m_keyCallbacks)
        callback(key, scancode, action, mods);
}

void Window::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    Window* thisWindow = static_cast<Window*>(glfwGetWindowUserPointer(window));

    for (auto& callback : thisWindow->m_mouseButtonCallbacks)
        callback(button, action, mods);
}

void Window::mouseMoveCallback(GLFWwindow* window, double xpos, double ypos)
{
    Window* thisWindow = static_cast<Window*>(glfwGetWindowUserPointer(window));

    for (auto& callback : thisWindow->m_mouseMoveCallbacks)
        callback(glm::dvec2(xpos, ypos));
}

bool Window::isKeyDown(int key) const
{
    return glfwGetKey(m_window, key) == GLFW_PRESS;
}

void Window::setMouseCapture(bool capture)
{
    if (capture) {
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    } else {
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    glfwPollEvents();
}

uint32_t Window::getWidth() const
{
    return m_width;
}

uint32_t Window::getHeight() const
{
    return m_height;
}

}
