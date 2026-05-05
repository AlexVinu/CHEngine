#pragma once

#include "RenderGraphTypes.h"

namespace CHEngine {

	class IRGPassBuilder
	{
	public:
		virtual ~IRGPassBuilder() = default;

		// --- Textures ---
		virtual void ReadTexture(RGResourceHandle h) = 0;
		virtual void WriteTexture(RGResourceHandle h) = 0;
		virtual RGResourceHandle CreateTexture(const RGResourceDesc& desc) = 0;

		// --- Buffers ---
		virtual void ReadBuffer(RGResourceHandle h) = 0;
		virtual void WriteBuffer(RGResourceHandle h) = 0;
		virtual RGResourceHandle CreateBuffer(const RGResourceDesc& desc) = 0;
	};
}