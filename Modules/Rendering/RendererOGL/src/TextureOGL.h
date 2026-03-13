#pragma once

#include "Render/ITexture.h"

namespace CHModules {

	class TextureOGL : public CHEngine::ITexture
	{
	public:
		TextureOGL(const uint8_t* data, uint32_t width, uint32_t height, uint32_t channels);
		~TextureOGL();

		void Bind(uint32_t slot = 0) const override;
		void Unbind() const override;

		uint32_t GetWidth()  const override { return m_Width;  }
		uint32_t GetHeight() const override { return m_Height; }

	private:
		uint32_t m_RendererID = 0;
		uint32_t m_Width      = 0;
		uint32_t m_Height     = 0;
	};

}
