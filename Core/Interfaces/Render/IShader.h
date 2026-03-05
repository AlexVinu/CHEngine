#pragma once

#include<Core.h>
#include <Memory/String.h>

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

	protected:
		uint32_t m_RendererID = 0;
	};
}
