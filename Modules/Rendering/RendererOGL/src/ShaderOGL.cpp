#include <chepch.h>
#include "ShaderOGL.h"

#include <glad/glad.h>

namespace CHModules
{

	// ---------------------------------------------------------------------------
	// Static helper: compile + link a GLSL program.
	// Returns the program ID on success, 0 on failure.
	// ---------------------------------------------------------------------------
	GLuint ShaderOGL::CompileProgram(const CHEngine::String& vertexSrc, const CHEngine::String& fragmentSrc)
	{
		// ---- vertex shader ----
		GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
		const GLchar* source = vertexSrc.c_str();
		glShaderSource(vertexShader, 1, &source, 0);
		glCompileShader(vertexShader);

		GLint isCompiled = 0;
		glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &isCompiled);
		if (isCompiled == GL_FALSE)
		{
			GLint maxLength = 0;
			glGetShaderiv(vertexShader, GL_INFO_LOG_LENGTH, &maxLength);
			std::vector<GLchar> infoLog(maxLength);
			glGetShaderInfoLog(vertexShader, maxLength, &maxLength, infoLog.data());
			glDeleteShader(vertexShader);
			CHE_CORE_ERROR("Vertex shader compilation failed: {0}", infoLog.data());
			return 0;
		}

		// ---- fragment shader ----
		GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
		source = fragmentSrc.c_str();
		glShaderSource(fragmentShader, 1, &source, 0);
		glCompileShader(fragmentShader);

		glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &isCompiled);
		if (isCompiled == GL_FALSE)
		{
			GLint maxLength = 0;
			glGetShaderiv(fragmentShader, GL_INFO_LOG_LENGTH, &maxLength);
			std::vector<GLchar> infoLog(maxLength);
			glGetShaderInfoLog(fragmentShader, maxLength, &maxLength, infoLog.data());
			glDeleteShader(fragmentShader);
			glDeleteShader(vertexShader);
			CHE_CORE_ERROR("Fragment shader compilation failed: {0}", infoLog.data());
			return 0;
		}

		// ---- link program ----
		GLuint program = glCreateProgram();
		glAttachShader(program, vertexShader);
		glAttachShader(program, fragmentShader);
		glLinkProgram(program);

		GLint isLinked = 0;
		glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
		if (isLinked == GL_FALSE)
		{
			GLint maxLength = 0;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
			std::vector<GLchar> infoLog(maxLength);
			glGetProgramInfoLog(program, maxLength, &maxLength, infoLog.data());
			glDeleteProgram(program);
			glDeleteShader(fragmentShader);
			glDeleteShader(vertexShader);
			CHE_CORE_ERROR("Shader link failed: {0}", infoLog.data());
			return 0;
		}

		glDetachShader(program, vertexShader);
		glDetachShader(program, fragmentShader);
		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);

		return program;
	}

	// ---------------------------------------------------------------------------

	ShaderOGL::ShaderOGL(const CHEngine::String& vertexSrc, const CHEngine::String& fragmentSrc)
	{
		m_RendererID = CompileProgram(vertexSrc, fragmentSrc);
		CHE_CORE_ASSERT(m_RendererID, "ShaderOGL: failed to compile/link shader");
	}

	ShaderOGL::~ShaderOGL()
	{
		glDeleteProgram(m_RendererID);
	}

	void ShaderOGL::Bind() const
	{
		glUseProgram(m_RendererID);
	}

	void ShaderOGL::Unbind() const
	{
		glUseProgram(0);
	}

	bool ShaderOGL::Reload(const CHEngine::String& vertexSrc, const CHEngine::String& fragmentSrc)
	{
		GLuint newProgram = CompileProgram(vertexSrc, fragmentSrc);
		if (!newProgram)
			return false;

		glDeleteProgram(m_RendererID);
		m_RendererID = newProgram;
		return true;
	}

	// ---------------------------------------------------------------------------
	// Uniform setters — shader must be bound before calling these
	// ---------------------------------------------------------------------------

	void ShaderOGL::SetInt(const CHEngine::String& name, int value)
	{
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniform1i(location, value);
	}

	void ShaderOGL::SetFloat(const CHEngine::String& name, float value)
	{
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniform1f(location, value);
	}

	void ShaderOGL::SetFloat3(const CHEngine::String& name, float x, float y, float z)
	{
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniform3f(location, x, y, z);
	}

	void ShaderOGL::SetFloat4(const CHEngine::String& name, float x, float y, float z, float w)
	{
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniform4f(location, x, y, z, w);
	}

	void ShaderOGL::SetMat4(const CHEngine::String& name, const float* matrix)
	{
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		// GL_FALSE = matrix is already column-major (GLM default)
		glUniformMatrix4fv(location, 1, GL_FALSE, matrix);
	}

}
