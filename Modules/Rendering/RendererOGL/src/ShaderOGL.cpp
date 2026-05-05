#include <chepch.h>
#include "ShaderOGL.h"

#include <Render/UniformBlocks.h>

#include <SlangBackend/SlangBackend.h>

#include <glad/glad.h>

#include <string>
#include <vector>

namespace CHModules
{

	namespace
	{
		// Compile one GLSL stage. Returns the GL shader id, or 0 on failure.
		GLuint CompileStage(GLenum stage, const std::string& source, const char* stageName)
		{
			GLuint shader = glCreateShader(stage);
			const GLchar* src = source.c_str();
			const GLint len = static_cast<GLint>(source.size());
			glShaderSource(shader, 1, &src, &len);
			glCompileShader(shader);

			GLint isCompiled = 0;
			glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
			if (isCompiled == GL_FALSE)
			{
				GLint maxLength = 0;
				glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);
				std::vector<GLchar> infoLog(maxLength);
				glGetShaderInfoLog(shader, maxLength, &maxLength, infoLog.data());
				glDeleteShader(shader);
				CHE_CORE_ERROR("ShaderOGL: {0} compilation failed: {1}", stageName, infoLog.data());
				return 0;
			}
			return shader;
		}
	}

	GLuint ShaderOGL::CompileSlangProgram(const CHEngine::String& slangSource,
	                                      const CHEngine::String& vertEntry,
	                                      const CHEngine::String& fragEntry,
	                                      const CHEngine::String& sourcePath)
	{
		SlangBackend* backend = SlangBackend::GetForApi(CHEngine::ERenderAPI::OPENGL);
		if (!backend)
		{
			CHE_CORE_ERROR("ShaderOGL: SlangBackend unavailable for OpenGL");
			return 0;
		}

		CompiledShader compiled = backend->Compile(slangSource, vertEntry, fragEntry, sourcePath);
		if (!compiled.valid)
		{
			CHE_CORE_ERROR("ShaderOGL: Slang compilation failed:\n{0}", compiled.errorLog);
			return 0;
		}

		// Slang выдаёт GLSL как текст без терминирующего нуля — берём как есть по размеру.
		std::string vertGlsl(reinterpret_cast<const char*>(compiled.vertexCode.data()),
		                     compiled.vertexCode.size());
		std::string fragGlsl(reinterpret_cast<const char*>(compiled.fragmentCode.data()),
		                     compiled.fragmentCode.size());

		GLuint vertexShader = CompileStage(GL_VERTEX_SHADER, vertGlsl, "vertex");
		if (!vertexShader)
			return 0;

		GLuint fragmentShader = CompileStage(GL_FRAGMENT_SHADER, fragGlsl, "fragment");
		if (!fragmentShader)
		{
			glDeleteShader(vertexShader);
			return 0;
		}

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
			CHE_CORE_ERROR("ShaderOGL: link failed: {0}", infoLog.data());
			return 0;
		}

		glDetachShader(program, vertexShader);
		glDetachShader(program, fragmentShader);
		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);

		return program;
	}

	void ShaderOGL::BindUBOBlocks()
	{
		// Slang mangles ConstantBuffer<CameraUBO> to "block_CameraUBO_0" etc.
		// We try both the plain name (for any future non-Slang path) and the
		// Slang-mangled name so the binding is always established correctly.
		struct BlockBinding { const char* names[2]; GLuint binding; };
		static const BlockBinding blocks[] = {
			{ { "CameraUBO",   "block_CameraUBO_0"   }, 0 },
			{ { "ObjectUBO",   "block_ObjectUBO_0"   }, 1 },
			{ { "LightingUBO", "block_LightingUBO_0" }, 2 },
			{ { "MaterialUBO", "block_MaterialUBO_0" }, 3 },
		};

		for (const auto& b : blocks) {
			for (const char* name : b.names) {
				GLuint idx = glGetUniformBlockIndex(m_ProgramID, name);
				if (idx != GL_INVALID_INDEX) {
					glUniformBlockBinding(m_ProgramID, idx, b.binding);
					break;
				}
			}
		}

		BindSamplerUnits();
	}

	void ShaderOGL::BindSamplerUnits()
	{
		// Slang emits GLSL samplers with target-assigned binding indices that
		// don't necessarily match texture unit 0,1,2,... in declaration order.
		// Enumerate active sampler uniforms post-link and explicitly assign
		// each one to a sequential texture unit, so callers can simply bind
		// textures to GL_TEXTUREN where N is the declaration order.

		GLint activeUniforms = 0;
		glGetProgramiv(m_ProgramID, GL_ACTIVE_UNIFORMS, &activeUniforms);
		if (activeUniforms <= 0)
			return;

		GLint maxNameLen = 0;
		glGetProgramiv(m_ProgramID, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxNameLen);
		if (maxNameLen <= 0)
			return;

		std::vector<char> nameBuf(static_cast<size_t>(maxNameLen));
		glUseProgram(m_ProgramID); // glUniform1i operates on the currently bound program

		uint32_t samplerUnit = 0;
		for (GLint i = 0; i < activeUniforms; ++i) {
			GLsizei len  = 0;
			GLint   size = 0;
			GLenum  type = 0;
			glGetActiveUniform(m_ProgramID, static_cast<GLuint>(i),
			                   maxNameLen, &len, &size, &type, nameBuf.data());

			const bool isSampler =
				type == GL_SAMPLER_2D            || type == GL_SAMPLER_2D_ARRAY ||
				type == GL_SAMPLER_2D_SHADOW     || type == GL_SAMPLER_CUBE     ||
				type == GL_SAMPLER_CUBE_SHADOW   || type == GL_SAMPLER_3D       ||
				type == GL_INT_SAMPLER_2D        || type == GL_UNSIGNED_INT_SAMPLER_2D;

			if (!isSampler)
				continue;

			GLint loc = glGetUniformLocation(m_ProgramID, nameBuf.data());
			if (loc == -1)
				continue;

			glUniform1i(loc, static_cast<GLint>(samplerUnit));
			++samplerUnit;
		}

		glUseProgram(0);
	}

	ShaderOGL::ShaderOGL(const CHEngine::String& slangSource,
	                     const CHEngine::String& vertEntry,
	                     const CHEngine::String& fragEntry,
	                     const CHEngine::String& sourcePath)
	{
		m_ProgramID = CompileSlangProgram(slangSource, vertEntry, fragEntry, sourcePath);
		CHE_CORE_ASSERT(m_ProgramID, "ShaderOGL: failed to compile/link shader");

		glGenBuffers(UBO_COUNT, m_UBOs.data());

		const uint32_t uboSizes[UBO_COUNT] = {
			static_cast<uint32_t>(sizeof(CHEngine::UBOCamera)),
			static_cast<uint32_t>(sizeof(CHEngine::UBOObject)),
			static_cast<uint32_t>(sizeof(CHEngine::UBOLighting)),
			static_cast<uint32_t>(sizeof(CHEngine::UBOMaterial)),
		};

		for (uint32_t i = 0; i < UBO_COUNT; ++i) {
			glBindBuffer(GL_UNIFORM_BUFFER, m_UBOs[i]);
			glBufferData(GL_UNIFORM_BUFFER, uboSizes[i], nullptr, GL_DYNAMIC_DRAW);
		}
		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		BindUBOBlocks();
	}

	ShaderOGL::~ShaderOGL()
	{
		glDeleteBuffers(UBO_COUNT, m_UBOs.data());
		glDeleteProgram(m_ProgramID);
	}

	void ShaderOGL::Bind() const
	{
		glUseProgram(m_ProgramID);
	}

	void ShaderOGL::Unbind() const
	{
		glUseProgram(0);
	}

	bool ShaderOGL::Reload(const CHEngine::String& slangSource,
	                       const CHEngine::String& vertEntry,
	                       const CHEngine::String& fragEntry,
	                       const CHEngine::String& sourcePath)
	{
		GLuint newProgram = CompileSlangProgram(slangSource, vertEntry, fragEntry, sourcePath);
		if (!newProgram)
			return false;

		glDeleteProgram(m_ProgramID);
		m_ProgramID = newProgram;

		BindUBOBlocks();
		return true;
	}

	void ShaderOGL::SetUniformBlock(CHEngine::EUniformBlock block, const void* data, uint32_t size)
	{
		uint32_t binding = static_cast<uint32_t>(block);
		if (binding >= UBO_COUNT) return;

		glBindBuffer(GL_UNIFORM_BUFFER, m_UBOs[binding]);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, size, data);
		glBindBufferBase(GL_UNIFORM_BUFFER, binding, m_UBOs[binding]);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	void ShaderOGL::SetInt(const CHEngine::String& name, int value)
	{
		GLint location = glGetUniformLocation(m_ProgramID, name.c_str());
		if (location == -1) return;
		glUniform1i(location, value);
	}

}
