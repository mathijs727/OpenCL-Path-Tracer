#pragma once
#include "opencl/cl_gl_includes.h" // Includes GLEW
#include <filesystem>

class GLOutput {
public:
    GLOutput(int width, int height);
    ~GLOutput();

    void render();
    inline GLuint getGLTexture() const { return m_texture; };

private:
    GLuint loadShader(const std::filesystem::path& filePath, GLenum type);

private:
    GLuint m_texture;
    GLuint m_vbo, m_vao;
    GLuint m_shader;
};
