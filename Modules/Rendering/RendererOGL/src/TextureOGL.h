#pragma once

#include "Render/Core/RenderTypes.h"

namespace CHModules {

	class TextureOGL
	{
	public:
		TextureOGL(const uint8_t* data,
			uint32_t width,
			uint32_t height,
			uint32_t channels,
			uint32_t mipLevels,
			uint32_t arrayLayers,
			CHEngine::TextureFormat format,
			CHEngine::TextureType type,
			CHEngine::TextureUsage usage,
			CHEngine::MemoryType memory,
			const char* debugName);

		~TextureOGL();

		void Bind(uint32_t slot = 0) const ;
		void Unbind() const ;

		uint32_t GetWidth()  const  { return m_Width;  }
		uint32_t GetHeight() const  { return m_Height; }
		uint32_t GetRendererID() const { return m_RendererID; }

	private:
		uint32_t m_RendererID = 0;
		uint32_t m_Width      = 0;
		uint32_t m_Height     = 0;
		uint32_t m_ArrayLayers = 1;
		CHEngine::TextureType m_Type;
		CHEngine::TextureFormat m_Format;
	};

}
