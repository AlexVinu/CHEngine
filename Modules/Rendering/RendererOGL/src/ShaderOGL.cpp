#include <chepch.h>
#include "ShaderOGL.h"

#include <Render/UniformBlocks.h>

#include <glad/glad.h>

#include <algorithm>

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
	// Привязка UBO uniform block → binding point
	// ---------------------------------------------------------------------------
	void ShaderOGL::BindUBOBlocks()
	{
		struct BlockBinding { const char* name; GLuint binding; };
		static const BlockBinding blocks[] = {
			{ "CameraUBO",   0 },
			{ "ObjectUBO",   1 },
			{ "LightingUBO", 2 },
		};

		for (const auto& b : blocks) {
			GLuint idx = glGetUniformBlockIndex(m_RendererID, b.name);
			if (idx != GL_INVALID_INDEX)
				glUniformBlockBinding(m_RendererID, idx, b.binding);
		}
	}

	// ---------------------------------------------------------------------------

	ShaderOGL::ShaderOGL(const CHEngine::String& vertexSrc, const CHEngine::String& fragmentSrc)
	{
		m_RendererID = CompileProgram(vertexSrc, fragmentSrc);
		CHE_CORE_ASSERT(m_RendererID, "ShaderOGL: failed to compile/link shader");

		// Создаём UBO-буферы
		glGenBuffers(UBO_COUNT, m_UBOs);

		const uint32_t uboSizes[UBO_COUNT] = {
			static_cast<uint32_t>(sizeof(CHEngine::UBOCamera)),
			static_cast<uint32_t>(sizeof(CHEngine::UBOObject)),
			static_cast<uint32_t>(sizeof(CHEngine::UBOLighting)),
		};

		for (uint32_t i = 0; i < UBO_COUNT; ++i) {
			glBindBuffer(GL_UNIFORM_BUFFER, m_UBOs[i]);
			glBufferData(GL_UNIFORM_BUFFER, uboSizes[i], nullptr, GL_DYNAMIC_DRAW);
		}
		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		// Связываем uniform blocks с binding points
		BindUBOBlocks();
	}

	ShaderOGL::~ShaderOGL()
	{
		glDeleteBuffers(UBO_COUNT, m_UBOs);
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

		// Повторно связываем UBO blocks для нового program
		BindUBOBlocks();
		return true;
	}

	// ---------------------------------------------------------------------------
	// UBO — обновление данных и привязка к binding point
	// ---------------------------------------------------------------------------

	void ShaderOGL::SetUniformBlock(CHEngine::EUniformBlock block, const void* data, uint32_t size)
	{
		uint32_t binding = static_cast<uint32_t>(block);
		if (binding >= UBO_COUNT) return;

		glBindBuffer(GL_UNIFORM_BUFFER, m_UBOs[binding]);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, size, data);
		glBindBufferBase(GL_UNIFORM_BUFFER, binding, m_UBOs[binding]);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	// ---------------------------------------------------------------------------
	// SetInt — для привязки текстурных sampler'ов (sampler нельзя в UBO)
	// ---------------------------------------------------------------------------

	void ShaderOGL::SetInt(const CHEngine::String& name, int value)
	{
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		if (location == -1) return;
		glUniform1i(location, value);
	}

}
