#pragma once

#include <Core.h>
#include <Containers/String.h>

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

		// --- Uniform setters ---
		// Shader must be bound (Bind()) before calling these.
		virtual void SetInt   (const String& name, int value) = 0;
		virtual void SetFloat (const String& name, float value) = 0;
		virtual void SetFloat3(const String& name, float x, float y, float z) = 0;
		virtual void SetFloat4(const String& name, float x, float y, float z, float w) = 0;
		// matrix: pointer to 16 floats in column-major order (GLM default)
		virtual void SetMat4  (const String& name, const float* matrix) = 0;

	protected:
		uint32_t m_RendererID = 0;
	};
}
