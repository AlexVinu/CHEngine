#pragma once

#include <Core.h>

namespace CHEngine
{
	class IRenderer
	{
	public:

		virtual ~IRenderer() = default;

		// Initialize renderer state for the given viewport size.
		virtual void Init(uint32_t width, uint32_t height) = 0;
		virtual void Shutdown() = 0;

		// Configure render viewport (in pixels).
		virtual void SetViewport(uint32_t width, uint32_t height) = 0;
	};
}