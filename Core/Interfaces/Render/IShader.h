#pragma once

#include <Core.h>
#include <Containers/String.h>
#include <Render/UniformBlocks.h>

namespace CHEngine {

	class IShader
	{
	public:
		virtual ~IShader() = default;

		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		// Recompile shader in-place from new source strings.
		// Returns true on success; on failure the old program remains active.
		virtual bool Reload(const String& vertexSrc, const String& fragmentSrc) = 0;

		// --- Uniform Buffer Objects ---
		// Заполняем структуру (UBOCamera / UBOObject / UBOLighting / UBOMaterial),
		// отправляем одним вызовом. Shader должен быть Bind().
		virtual void SetUniformBlock(EUniformBlock block, const void* data, uint32_t size) = 0;

		// --- Одиночные uniform'ы (для текстурных sampler'ов) ---
		// Sampler'ы нельзя поместить в UBO, поэтому SetInt остаётся.
		virtual void SetInt(const String& name, int value) = 0;

	protected:
		uint32_t m_RendererID = 0;
	};
}
