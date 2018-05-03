#pragma once
#include "template/cl_gl_includes.h"// Includes GLEW

class GLOutput
{
public:
	GLOutput();
	~GLOutput();

	void Init(int width, int height);
	void Render();
	inline GLuint GetGLTexture() const { return m_texture; };
private:
	GLuint LoadShader(const char* fileName, GLenum type);
private:
	bool _initialized;

	GLuint m_texture;
	GLuint m_vbo, m_vao;
	GLuint m_shader;
};