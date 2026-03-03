#pragma once

#include <Render/IShader.h>

#include <Memory/String.h>

namespace CHModules
{
	class ShaderOGL : public CHEngine::IShader
	{
	public:
		ShaderOGL(const CHEngine::String& vertexSrc, const CHEngine::String& fragmentSrc);
		virtual ~ShaderOGL();

		// Запрещаем копирование
		ShaderOGL(const ShaderOGL&) = delete;
		ShaderOGL& operator=(const ShaderOGL&) = delete;

		// 
		ShaderOGL(ShaderOGL&& other) = delete;
		ShaderOGL& operator=(ShaderOGL&& other) = delete;

		virtual void Bind() const override;
		virtual void Unbind() const override;
	};
}
