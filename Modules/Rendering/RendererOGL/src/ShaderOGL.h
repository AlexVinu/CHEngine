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

		virtual void SetInt   (const CHEngine::String& name, int value) override;
		virtual void SetFloat (const CHEngine::String& name, float value) override;
		virtual void SetFloat3(const CHEngine::String& name, float x, float y, float z) override;
		virtual void SetFloat4(const CHEngine::String& name, float x, float y, float z, float w) override;
		virtual void SetMat4  (const CHEngine::String& name, const float* matrix) override;

	private:
		// Compiles and links a program; returns program ID on success, 0 on failure.
		static GLuint CompileProgram(const CHEngine::String& vertexSrc, const CHEngine::String& fragmentSrc);
	};
}
