#include "TextureOGL.h"

#include <glad/glad.h>

namespace CHModules {

	TextureOGL::TextureOGL(const uint8_t* data, uint32_t width, uint32_t height, uint32_t channels)
		: m_Width(width), m_Height(height)
	{
		GLenum internalFormat = GL_RGBA8;
		GLenum dataFormat     = GL_RGBA;

		if (channels == 3) { internalFormat = GL_RGB8;  dataFormat = GL_RGB;  }
		else if (channels == 1) { internalFormat = GL_R8; dataFormat = GL_RED; }

		glGenTextures(1, &m_RendererID);
		glBindTexture(GL_TEXTURE_2D, m_RendererID);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, (GLsizei)width, (GLsizei)height,
		             0, dataFormat, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		glBindTexture(GL_TEXTURE_2D, 0);
	}

	TextureOGL::~TextureOGL()
	{
		if (m_RendererID)
			glDeleteTextures(1, &m_RendererID);
	}

	void TextureOGL::Bind(uint32_t slot) const
	{
		glActiveTexture(GL_TEXTURE0 + slot);
		glBindTexture(GL_TEXTURE_2D, m_RendererID);
	}

	void TextureOGL::Unbind() const
	{
		glBindTexture(GL_TEXTURE_2D, 0);
	}

}
