#pragma once
#define GLEW_STATIC
#include <OpenGL/glew.h>

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
	GLuint m_texture;
	GLuint m_vbo, m_vao;
	GLuint m_shader;
};