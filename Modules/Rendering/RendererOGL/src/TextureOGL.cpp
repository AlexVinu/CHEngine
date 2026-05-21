#include "TextureOGL.h"

#include <cassert>
#include <glad/glad.h>

// Extension format constants not exposed by base GLAD core profile loader.
// Values come from GL_EXT_texture_compression_s3tc and GL_KHR_texture_compression_astc_*.
#ifndef GL_COMPRESSED_RGB_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT        0x83F0
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT       0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT       0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT       0x83F3
#endif
#ifndef GL_COMPRESSED_SRGB_S3TC_DXT1_EXT
#define GL_COMPRESSED_SRGB_S3TC_DXT1_EXT       0x8C4C
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT 0x8C4D
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT 0x8C4E
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT 0x8C4F
#endif
#ifndef GL_COMPRESSED_RGBA_ASTC_4x4_KHR
#define GL_COMPRESSED_RGBA_ASTC_4x4_KHR        0x93B0
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR 0x93D0
#endif

namespace CHModules {

	struct GLFormatInfo {
		GLenum internalFormat;
		GLenum dataFormat;
		GLenum dataType;
		bool compressed;
	};

	static GLFormatInfo GetGLFormat(CHEngine::TextureFormat format) {
		switch (format) {
			// 8-bit UNORM
		case CHEngine::TextureFormat::R8_UNORM:
			return { GL_R8, GL_RED, GL_UNSIGNED_BYTE, false };
		case CHEngine::TextureFormat::R8_SNORM:
			return { GL_R8_SNORM, GL_RED, GL_BYTE, false };
		case CHEngine::TextureFormat::R8_UINT:
			return { GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE, false };
		case CHEngine::TextureFormat::R8_SINT:
			return { GL_R8I, GL_RED_INTEGER, GL_BYTE, false };

		case CHEngine::TextureFormat::RG8_UNORM:
			return { GL_RG8, GL_RG, GL_UNSIGNED_BYTE, false };
		case CHEngine::TextureFormat::RG8_SNORM:
			return { GL_RG8_SNORM, GL_RG, GL_BYTE, false };
		case CHEngine::TextureFormat::RG8_UINT:
			return { GL_RG8UI, GL_RG_INTEGER, GL_UNSIGNED_BYTE, false };
		case CHEngine::TextureFormat::RG8_SINT:
			return { GL_RG8I, GL_RG_INTEGER, GL_BYTE, false };

		case CHEngine::TextureFormat::RGBA8_UNORM:
			return { GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, false };
		case CHEngine::TextureFormat::RGBA8_UNORM_SRGB:
			return { GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, false };
		case CHEngine::TextureFormat::RGBA8_SNORM:
			return { GL_RGBA8_SNORM, GL_RGBA, GL_BYTE, false };
		case CHEngine::TextureFormat::RGBA8_UINT:
			return { GL_RGBA8UI, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, false };
		case CHEngine::TextureFormat::RGBA8_SINT:
			return { GL_RGBA8I, GL_RGBA_INTEGER, GL_BYTE, false };

		case CHEngine::TextureFormat::BGRA8_UNORM:
			return { GL_RGBA8, GL_BGRA, GL_UNSIGNED_BYTE, false };
		case CHEngine::TextureFormat::BGRA8_UNORM_SRGB:
			return { GL_SRGB8_ALPHA8, GL_BGRA, GL_UNSIGNED_BYTE, false };

			// 16-bit
		case CHEngine::TextureFormat::R16_UNORM:
			return { GL_R16, GL_RED, GL_UNSIGNED_SHORT, false };
		case CHEngine::TextureFormat::R16_SNORM:
			return { GL_R16_SNORM, GL_RED, GL_SHORT, false };
		case CHEngine::TextureFormat::R16_UINT:
			return { GL_R16UI, GL_RED_INTEGER, GL_UNSIGNED_SHORT, false };
		case CHEngine::TextureFormat::R16_SINT:
			return { GL_R16I, GL_RED_INTEGER, GL_SHORT, false };
		case CHEngine::TextureFormat::R16_FLOAT:
			return { GL_R16F, GL_RED, GL_HALF_FLOAT, false };

		case CHEngine::TextureFormat::RG16_UNORM:
			return { GL_RG16, GL_RG, GL_UNSIGNED_SHORT, false };
		case CHEngine::TextureFormat::RG16_SNORM:
			return { GL_RG16_SNORM, GL_RG, GL_SHORT, false };
		case CHEngine::TextureFormat::RG16_UINT:
			return { GL_RG16UI, GL_RG_INTEGER, GL_UNSIGNED_SHORT, false };
		case CHEngine::TextureFormat::RG16_SINT:
			return { GL_RG16I, GL_RG_INTEGER, GL_SHORT, false };
		case CHEngine::TextureFormat::RG16_FLOAT:
			return { GL_RG16F, GL_RG, GL_HALF_FLOAT, false };

		case CHEngine::TextureFormat::RGBA16_UNORM:
			return { GL_RGBA16, GL_RGBA, GL_UNSIGNED_SHORT, false };
		case CHEngine::TextureFormat::RGBA16_SNORM:
			return { GL_RGBA16_SNORM, GL_RGBA, GL_SHORT, false };
		case CHEngine::TextureFormat::RGBA16_UINT:
			return { GL_RGBA16UI, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, false };
		case CHEngine::TextureFormat::RGBA16_SINT:
			return { GL_RGBA16I, GL_RGBA_INTEGER, GL_SHORT, false };
		case CHEngine::TextureFormat::RGBA16_FLOAT:
			return { GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, false };

			// 32-bit
		case CHEngine::TextureFormat::R32_UINT:
			return { GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, false };
		case CHEngine::TextureFormat::R32_SINT:
			return { GL_R32I, GL_RED_INTEGER, GL_INT, false };
		case CHEngine::TextureFormat::R32_FLOAT:
			return { GL_R32F, GL_RED, GL_FLOAT, false };

		case CHEngine::TextureFormat::RG32_UINT:
			return { GL_RG32UI, GL_RG_INTEGER, GL_UNSIGNED_INT, false };
		case CHEngine::TextureFormat::RG32_SINT:
			return { GL_RG32I, GL_RG_INTEGER, GL_INT, false };
		case CHEngine::TextureFormat::RG32_FLOAT:
			return { GL_RG32F, GL_RG, GL_FLOAT, false };

		case CHEngine::TextureFormat::RGB32_UINT:
			return { GL_RGB32UI, GL_RGB_INTEGER, GL_UNSIGNED_INT, false };
		case CHEngine::TextureFormat::RGB32_SINT:
			return { GL_RGB32I, GL_RGB_INTEGER, GL_INT, false };
		case CHEngine::TextureFormat::RGB32_FLOAT:
			return { GL_RGB32F, GL_RGB, GL_FLOAT, false };

		case CHEngine::TextureFormat::RGBA32_UINT:
			return { GL_RGBA32UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT, false };
		case CHEngine::TextureFormat::RGBA32_SINT:
			return { GL_RGBA32I, GL_RGBA_INTEGER, GL_INT, false };
		case CHEngine::TextureFormat::RGBA32_FLOAT:
			return { GL_RGBA32F, GL_RGBA, GL_FLOAT, false };

			// Special
		case CHEngine::TextureFormat::RGB10_A2_UNORM:
			return { GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, false };
		case CHEngine::TextureFormat::RGB10_A2_UINT:
			return { GL_RGB10_A2UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT_2_10_10_10_REV, false };

		case CHEngine::TextureFormat::R11G11B10_FLOAT:
			return { GL_R11F_G11F_B10F, GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV, false };

		case CHEngine::TextureFormat::RGB9_E5_FLOAT:
			return { GL_RGB9_E5, GL_RGB, GL_UNSIGNED_INT_5_9_9_9_REV, false };

			// Compressed formats
		case CHEngine::TextureFormat::BC1_RGB_UNORM:
			return { GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 0, 0, true };
		case CHEngine::TextureFormat::BC1_RGB_SRGB:
			return { GL_COMPRESSED_SRGB_S3TC_DXT1_EXT, 0, 0, true };
		case CHEngine::TextureFormat::BC1_RGBA_UNORM:
			return { GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 0, 0, true };
		case CHEngine::TextureFormat::BC1_RGBA_SRGB:
			return { GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT, 0, 0, true };

		case CHEngine::TextureFormat::BC2_UNORM:
			return { GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, 0, 0, true };
		case CHEngine::TextureFormat::BC2_SRGB:
			return { GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT, 0, 0, true };

		case CHEngine::TextureFormat::BC3_UNORM:
			return { GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, 0, 0, true };
		case CHEngine::TextureFormat::BC3_SRGB:
			return { GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT, 0, 0, true };

		case CHEngine::TextureFormat::BC4_UNORM:
			return { GL_COMPRESSED_RED_RGTC1, 0, 0, true };
		case CHEngine::TextureFormat::BC4_SNORM:
			return { GL_COMPRESSED_SIGNED_RED_RGTC1, 0, 0, true };

		case CHEngine::TextureFormat::BC5_UNORM:
			return { GL_COMPRESSED_RG_RGTC2, 0, 0, true };
		case CHEngine::TextureFormat::BC5_SNORM:
			return { GL_COMPRESSED_SIGNED_RG_RGTC2, 0, 0, true };

		case CHEngine::TextureFormat::BC6H_UFLOAT:
			return { GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT, 0, 0, true };
		case CHEngine::TextureFormat::BC6H_SFLOAT:
			return { GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT, 0, 0, true };

		case CHEngine::TextureFormat::BC7_UNORM:
			return { GL_COMPRESSED_RGBA_BPTC_UNORM, 0, 0, true };
		case CHEngine::TextureFormat::BC7_SRGB:
			return { GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM, 0, 0, true };

			// ASTC
		case CHEngine::TextureFormat::ASTC_4x4_UNORM:
			return { GL_COMPRESSED_RGBA_ASTC_4x4_KHR, 0, 0, true };
		case CHEngine::TextureFormat::ASTC_4x4_SRGB:
			return { GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR, 0, 0, true };
			// ... ��������� ASTC ������� �������� ����������

			// ETC2
		case CHEngine::TextureFormat::ETC2_R8G8B8_UNORM:
			return { GL_COMPRESSED_RGB8_ETC2, 0, 0, true };
		case CHEngine::TextureFormat::ETC2_R8G8B8_SRGB:
			return { GL_COMPRESSED_SRGB8_ETC2, 0, 0, true };
		case CHEngine::TextureFormat::ETC2_R8G8B8A1_UNORM:
			return { GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2, 0, 0, true };
		case CHEngine::TextureFormat::ETC2_R8G8B8A1_SRGB:
			return { GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2, 0, 0, true };
		case CHEngine::TextureFormat::ETC2_R8G8B8A8_UNORM:
			return { GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0, true };
		case CHEngine::TextureFormat::ETC2_R8G8B8A8_SRGB:
			return { GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC, 0, 0, true };
		case CHEngine::TextureFormat::EAC_R11_UNORM:
			return { GL_COMPRESSED_R11_EAC, 0, 0, true };
		case CHEngine::TextureFormat::EAC_R11_SNORM:
			return { GL_COMPRESSED_SIGNED_R11_EAC, 0, 0, true };
		case CHEngine::TextureFormat::EAC_RG11_UNORM:
			return { GL_COMPRESSED_RG11_EAC, 0, 0, true };
		case CHEngine::TextureFormat::EAC_RG11_SNORM:
			return { GL_COMPRESSED_SIGNED_RG11_EAC, 0, 0, true };

			// Depth/Stencil
		case CHEngine::TextureFormat::D16_UNORM:
			return { GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, false };
		case CHEngine::TextureFormat::D24_UNORM_S8_UINT:
			return { GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, false };
		case CHEngine::TextureFormat::D32_FLOAT:
			return { GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, false };
		case CHEngine::TextureFormat::D32_FLOAT_S8_UINT:
			return { GL_DEPTH32F_STENCIL8, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV, false };
		case CHEngine::TextureFormat::S8_UINT:
			return { GL_STENCIL_INDEX8, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, false };

		default:
			// Fallback
			return { GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, false };
		}
	}

	TextureOGL::TextureOGL(const uint8_t* data,
		uint32_t width,
		uint32_t height,
		uint32_t channels,
		uint32_t mipLevels,
		uint32_t arrayLayers,
		CHEngine::TextureFormat format,
		CHEngine::TextureType type,
		CHEngine::TextureUsage usage,
		CHEngine::MemoryType memory,
		const char* debugName)
		: m_Width(width)
		, m_Height(height)
		, m_ArrayLayers(arrayLayers)
		, m_Type(type)
		, m_Format(format)
	{
		// �������� ���������� � �������
		GLFormatInfo formatInfo = GetGLFormat(format);

		// ���������� ������� ��� ��������
		GLenum target = GL_TEXTURE_2D;
		if (type == CHEngine::TextureType::Texture2DArray) {
			target = GL_TEXTURE_2D_ARRAY;
		}
		else if (type == CHEngine::TextureType::TextureCube) {
			target = GL_TEXTURE_CUBE_MAP;
			assert(arrayLayers == 6);
		}
		else if (type == CHEngine::TextureType::Texture3D) {
			target = GL_TEXTURE_3D;
		}

		// ���������� ��������
		glGenTextures(1, &m_RendererID);
		glBindTexture(target, m_RendererID);

		// ������������� ��������� ��������
		GLenum wrapS = GL_REPEAT;
		GLenum wrapT = GL_REPEAT;
		GLenum minFilter = GL_LINEAR;
		GLenum magFilter = GL_LINEAR;

		// ��� ������������� �������� ������ ������������ �������� ����������
		if (formatInfo.internalFormat == GL_R8UI || formatInfo.internalFormat == GL_R8I ||
			formatInfo.internalFormat == GL_R16UI || formatInfo.internalFormat == GL_R16I ||
			formatInfo.internalFormat == GL_R32UI || formatInfo.internalFormat == GL_R32I ||
			formatInfo.internalFormat == GL_RG8UI || formatInfo.internalFormat == GL_RG8I ||
			formatInfo.internalFormat == GL_RG16UI || formatInfo.internalFormat == GL_RG16I ||
			formatInfo.internalFormat == GL_RG32UI || formatInfo.internalFormat == GL_RG32I ||
			formatInfo.internalFormat == GL_RGBA8UI || formatInfo.internalFormat == GL_RGBA8I ||
			formatInfo.internalFormat == GL_RGBA16UI || formatInfo.internalFormat == GL_RGBA16I ||
			formatInfo.internalFormat == GL_RGBA32UI || formatInfo.internalFormat == GL_RGBA32I) {
			minFilter = GL_NEAREST;
			magFilter = GL_NEAREST;
		}

		// ��� depth/stencil �������� ������ ���������
		if (format == CHEngine::TextureFormat::D16_UNORM ||
			format == CHEngine::TextureFormat::D24_UNORM_S8_UINT ||
			format == CHEngine::TextureFormat::D32_FLOAT ||
			format == CHEngine::TextureFormat::D32_FLOAT_S8_UINT) {
			wrapS = GL_CLAMP_TO_EDGE;
			wrapT = GL_CLAMP_TO_EDGE;
			// ��� shadow sampling ����� ������������ GL_COMPARE_REF_TO_TEXTURE
		}

		glTexParameteri(target, GL_TEXTURE_WRAP_S, wrapS);
		glTexParameteri(target, GL_TEXTURE_WRAP_T, wrapT);
		if (target == GL_TEXTURE_2D_ARRAY || target == GL_TEXTURE_CUBE_MAP || target == GL_TEXTURE_3D) {
			glTexParameteri(target, GL_TEXTURE_WRAP_R, wrapS);
		}

		// ��������� mip-mapping
		if (mipLevels > 1 || mipLevels == 0) {
			minFilter = (minFilter == GL_NEAREST) ? GL_NEAREST_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_LINEAR;
		}

		glTexParameteri(target, GL_TEXTURE_MIN_FILTER, minFilter);
		glTexParameteri(target, GL_TEXTURE_MAG_FILTER, magFilter);

		// ��������� ������� ��� ������ ��������
		auto calculateBlockSize = [format](uint32_t size) -> uint32_t {
			switch (format) {
			case CHEngine::TextureFormat::BC1_RGB_UNORM:
			case CHEngine::TextureFormat::BC1_RGB_SRGB:
			case CHEngine::TextureFormat::BC1_RGBA_UNORM:
			case CHEngine::TextureFormat::BC1_RGBA_SRGB:
			case CHEngine::TextureFormat::BC4_UNORM:
			case CHEngine::TextureFormat::BC4_SNORM:
			case CHEngine::TextureFormat::ETC2_R8G8B8_UNORM:
			case CHEngine::TextureFormat::ETC2_R8G8B8_SRGB:
			case CHEngine::TextureFormat::ETC2_R8G8B8A1_UNORM:
			case CHEngine::TextureFormat::ETC2_R8G8B8A1_SRGB:
			case CHEngine::TextureFormat::EAC_R11_UNORM:
			case CHEngine::TextureFormat::EAC_R11_SNORM:
				return (size + 3) / 4 * 4; // 4x4 blocks
			case CHEngine::TextureFormat::ASTC_4x4_UNORM:
			case CHEngine::TextureFormat::ASTC_4x4_SRGB:
				return (size + 3) / 4 * 4;
			case CHEngine::TextureFormat::ASTC_5x4_UNORM:
			case CHEngine::TextureFormat::ASTC_5x4_SRGB:
				return ((size + 4) / 5) * 5;
			case CHEngine::TextureFormat::ASTC_5x5_UNORM:
			case CHEngine::TextureFormat::ASTC_5x5_SRGB:
				return ((size + 4) / 5) * 5;
			case CHEngine::TextureFormat::ASTC_6x5_UNORM:
			case CHEngine::TextureFormat::ASTC_6x5_SRGB:
				return ((size + 5) / 6) * 6;
			case CHEngine::TextureFormat::ASTC_6x6_UNORM:
			case CHEngine::TextureFormat::ASTC_6x6_SRGB:
				return ((size + 5) / 6) * 6;
			case CHEngine::TextureFormat::ASTC_8x5_UNORM:
			case CHEngine::TextureFormat::ASTC_8x5_SRGB:
				return ((size + 7) / 8) * 8;
			case CHEngine::TextureFormat::ASTC_8x6_UNORM:
			case CHEngine::TextureFormat::ASTC_8x6_SRGB:
				return ((size + 7) / 8) * 8;
			case CHEngine::TextureFormat::ASTC_8x8_UNORM:
			case CHEngine::TextureFormat::ASTC_8x8_SRGB:
				return ((size + 7) / 8) * 8;
			case CHEngine::TextureFormat::ASTC_10x5_UNORM:
			case CHEngine::TextureFormat::ASTC_10x5_SRGB:
				return ((size + 9) / 10) * 10;
			case CHEngine::TextureFormat::ASTC_10x6_UNORM:
			case CHEngine::TextureFormat::ASTC_10x6_SRGB:
				return ((size + 9) / 10) * 10;
			case CHEngine::TextureFormat::ASTC_10x8_UNORM:
			case CHEngine::TextureFormat::ASTC_10x8_SRGB:
				return ((size + 9) / 10) * 10;
			case CHEngine::TextureFormat::ASTC_10x10_UNORM:
			case CHEngine::TextureFormat::ASTC_10x10_SRGB:
				return ((size + 9) / 10) * 10;
			case CHEngine::TextureFormat::ASTC_12x10_UNORM:
			case CHEngine::TextureFormat::ASTC_12x10_SRGB:
				return ((size + 11) / 12) * 12;
			case CHEngine::TextureFormat::ASTC_12x12_UNORM:
			case CHEngine::TextureFormat::ASTC_12x12_SRGB:
				return ((size + 11) / 12) * 12;
			default:
				return size;
			}
			};

		uint32_t blockWidth = calculateBlockSize(width);
		uint32_t blockHeight = calculateBlockSize(height);

		// ��������� ������ ��������
		if (formatInfo.compressed) {
			// ��� ������ ��������
			if (target == GL_TEXTURE_2D) {
				glCompressedTexImage2D(GL_TEXTURE_2D, 0, formatInfo.internalFormat,
					blockWidth, blockHeight, 0,
					/* size of data */ width * height, data);
			}
			else if (target == GL_TEXTURE_2D_ARRAY) {
				glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, formatInfo.internalFormat,
					blockWidth, blockHeight, arrayLayers, 0,
					/* size of data */ width * height * arrayLayers, data);
			}
			else if (target == GL_TEXTURE_CUBE_MAP) {
				size_t faceSize = width * height; // ���������
				for (uint32_t face = 0; face < 6; ++face) {
					glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0,
						formatInfo.internalFormat,
						blockWidth, blockHeight, 0,
						faceSize, data + face * faceSize);
				}
			}
		}
		else {
			// ��� �������� ��������
			if (target == GL_TEXTURE_2D) {
				glTexImage2D(GL_TEXTURE_2D, 0, formatInfo.internalFormat,
					width, height, 0,
					formatInfo.dataFormat, formatInfo.dataType, data);
			}
			else if (target == GL_TEXTURE_2D_ARRAY) {
				glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, formatInfo.internalFormat,
					width, height, arrayLayers, 0,
					formatInfo.dataFormat, formatInfo.dataType, data);
			}
			else if (target == GL_TEXTURE_CUBE_MAP) {
				size_t faceSize = width * height * channels; // ������ ����� �����
				for (uint32_t face = 0; face < 6; ++face) {
					glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0,
						formatInfo.internalFormat,
						width, height, 0,
						formatInfo.dataFormat, formatInfo.dataType,
						data + face * faceSize);
				}
			}
		}

		// ��������� mip-�������
		if (mipLevels == 0) {
			// �������������� ��������� ���� �������
			glGenerateMipmap(target);
		}
		else if (mipLevels > 1) {
			glTexParameteri(target, GL_TEXTURE_BASE_LEVEL, 0);
			glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, mipLevels - 1);
			if (data) {
				glGenerateMipmap(target);
			}
		}

		// ���� usage ��� �����������
		if (usage == CHEngine::TextureUsage::RenderTarget ||
			usage == CHEngine::TextureUsage::DepthStencil) {
			// �� ���������� mipmap ��� render targets �� ���������
		}

		// ���� memory ��� ��������� ��������
		if (memory == CHEngine::MemoryType::GpuOnly) {
			// �������� ����� ����� �������������� GPU, ����� �������� ��� ����
		}

		// ���������� ���
		if (debugName && glObjectLabel) {
			glObjectLabel(GL_TEXTURE, m_RendererID, -1, debugName);
		}

		glBindTexture(target, 0);
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
