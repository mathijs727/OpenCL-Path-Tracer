#pragma once
#include "template/cl_gl_includes.h"// Includes GLEW

class GLOutput
{
public:
	GLOutput(int width, int height);
	~GLOutput();

	void render();
	inline GLuint getGLTexture() const { return m_texture; };
private:
	GLuint loadShader(const char* fileName, GLenum type);
private:
	GLuint m_texture;
	GLuint m_vbo, m_vao;
	GLuint m_shader;
};