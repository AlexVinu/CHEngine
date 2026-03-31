#pragma once

#include <Render/IShader.h>

#include <Containers/String.h>

#include <glad/glad.h>

namespace CHModules
{
	class ShaderOGL : public CHEngine::IShader
	{
	public:
		ShaderOGL(const CHEngine::String& vertexSrc, const CHEngine::String& fragmentSrc);
		virtual ~ShaderOGL();

		ShaderOGL(const ShaderOGL&) = delete;
		ShaderOGL& operator=(const ShaderOGL&) = delete;

		ShaderOGL(ShaderOGL&& other) = delete;
		ShaderOGL& operator=(ShaderOGL&& other) = delete;

		virtual void Bind() const override;
		virtual void Unbind() const override;
		virtual bool Reload(const CHEngine::String& vertexSrc, const CHEngine::String& fragmentSrc) override;

		// --- UBO ---
		virtual void SetUniformBlock(CHEngine::EUniformBlock block, const void* data, uint32_t size) override;

		// --- Sampler binding ---
		virtual void SetInt(const CHEngine::String& name, int value) override;

	private:
		// Compiles and links a program; returns program ID on success, 0 on failure.
		static GLuint CompileProgram(const CHEngine::String& vertexSrc, const CHEngine::String& fragmentSrc);

		// Привязка UBO block indices к binding points после компиляции/перекомпиляции
		void BindUBOBlocks();

		// OpenGL Uniform Buffer Objects (по одному на каждый binding point)
		static constexpr uint32_t UBO_COUNT = 4;
		GLuint m_UBOs[UBO_COUNT] = {0, 0, 0, 0};
	};
}
