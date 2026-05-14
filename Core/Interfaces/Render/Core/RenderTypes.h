#pragma once

#include <Core.h>
#include <cstdint>

namespace CHEngine {

	enum class VertexFormat : uint8_t {
		Float, Float2, Float3, Float4,
		Int, Int2, Int3, Int4,
		UInt, UInt2, UInt3, UInt4,
		UByte4, UByte4_Norm,
		Mat3, Mat4
	};

	inline uint32_t GetVertexFormatSize(VertexFormat format) {
		switch (format) {
		case VertexFormat::Float:       return 4;
		case VertexFormat::Float2:      return 8;
		case VertexFormat::Float3:      return 12;
		case VertexFormat::Float4:      return 16;
		case VertexFormat::Int:         return 4;
		case VertexFormat::Int2:        return 8;
		case VertexFormat::Int3:        return 12;
		case VertexFormat::Int4:        return 16;
		case VertexFormat::UInt:        return 4;
		case VertexFormat::UInt2:       return 8;
		case VertexFormat::UInt3:       return 12;
		case VertexFormat::UInt4:       return 16;
		case VertexFormat::UByte4:      return 4;
		case VertexFormat::UByte4_Norm: return 4;
		case VertexFormat::Mat3:        return 48; 
		case VertexFormat::Mat4:        return 64;
		}
		return 0;
	}

	enum class TextureFormat : uint32_t {
		// ==========================================
		// 8-bit на компонент (UNORM/SRGB)
		// ==========================================

		R8_UNORM,                // 1 канал,  8 бит,  [0..1]
		R8_SNORM,                // 1 канал,  8 бит,  [-1..1]
		R8_UINT,                 // 1 канал,  8 бит,  целое беззнаковое
		R8_SINT,                 // 1 канал,  8 бит,  целое знаковое

		RG8_UNORM,               // 2 канала, 8 бит каждый
		RG8_SNORM,
		RG8_UINT,
		RG8_SINT,

		RGBA8_UNORM,             // 4 канала, 8 бит каждый Ч самый частый
		RGBA8_UNORM_SRGB,        // “о же, но с гамма-коррекцией при чтении
		RGBA8_SNORM,
		RGBA8_UINT,
		RGBA8_SINT,

		BGRA8_UNORM,             // DDS-файлы часто в BGRA пор€дке
		BGRA8_UNORM_SRGB,

		// ==========================================
		// 16-bit на компонент
		// ==========================================

		R16_UNORM,               // 1 канал, 16 бит
		R16_SNORM,
		R16_UINT,
		R16_SINT,
		R16_FLOAT,               // ѕоловина float16 Ч дл€ HDR данных

		RG16_UNORM,
		RG16_SNORM,
		RG16_UINT,
		RG16_SINT,
		RG16_FLOAT,              // ƒва float16 Ч карты нормалей в HDR

		RGBA16_UNORM,
		RGBA16_SNORM,
		RGBA16_UINT,
		RGBA16_SINT,
		RGBA16_FLOAT,            // —амый частый формат дл€ HDR рендер-таргетов

		// ==========================================
		// 32-bit на компонент
		// ==========================================

		R32_UINT,                // „асто дл€ ID буферов, масок
		R32_SINT,
		R32_FLOAT,               // Ѕуферы глубины, данные дл€ compute

		RG32_UINT,
		RG32_SINT,
		RG32_FLOAT,              // 2? float32 Ч координаты, вектора

		RGB32_UINT,
		RGB32_SINT,
		RGB32_FLOAT,             // 3? float32 Ч позиции в мировом пространстве

		RGBA32_UINT,
		RGBA32_SINT,
		RGBA32_FLOAT,            // 4? float32 Ч максимальна€ точность

		// ==========================================
		// —пециальные форматы
		// ==========================================

		// 10:10:10:2 Ч дл€ нормалей и цветов с высокой точностью
		RGB10_A2_UNORM,          // 10 бит RGB, 2 бита Alpha
		RGB10_A2_UINT,

		// 11:11:10 Ч HDR без альфы (экономи€ пам€ти)
		R11G11B10_FLOAT,         // 11+11+10 бит float Ч часто дл€ HDR света

		// 9:9:9:5 Ч редко, но существует
		RGB9_E5_FLOAT,           // Shared exponent Ч дл€ HDR окружени€

		// ==========================================
		// —жатые форматы (Block Compression)
		// ==========================================

		// BC1 (DXT1) Ч RGB, 4?4 блоки, 8 байт на блок (6:1 сжатие)
		BC1_RGB_UNORM,           // Ѕез альфы
		BC1_RGB_SRGB,
		BC1_RGBA_UNORM,          // 1 бит альфы (прозрачный/непрозрачный)
		BC1_RGBA_SRGB,

		// BC2 (DXT3) Ч RGBA, 4?4 блоки, 16 байт на блок (4:1)
		BC2_UNORM,               // 4 бита альфы (16 уровней)
		BC2_SRGB,

		// BC3 (DXT5) Ч RGBA, 4?4 блоки, 16 байт на блок (4:1)
		BC3_UNORM,               // »нтерполированна€ альфа (8 уровней + интерпол€ци€)
		BC3_SRGB,

		// BC4 Ч 1 канал, 4?4 блоки, 8 байт на блок (2:1)
		BC4_UNORM,               // ќдин канал (например, roughness)
		BC4_SNORM,               // «наковый вариант

		// BC5 Ч 2 канала, 4?4 блоки, 16 байт на блок (2:1)
		BC5_UNORM,               //  арты нормалей (XY, Z восстанавливаетс€)
		BC5_SNORM,               // «наковые нормали

		// BC6H Ч HDR, 3 канала, 4?4 блоки, 16 байт на блок (6:1)
		BC6H_UFLOAT,             // Unsigned float HDR
		BC6H_SFLOAT,             // Signed float HDR (дл€ HDR окружени€)

		// BC7 Ч 4 канала, 4?4 блоки, 16 байт на блок (4:1)
		BC7_UNORM,               // ћаксимальное качество дл€ LDR
		BC7_SRGB,                // —амый частый дл€ албедо текстур

		// ASTC (Adaptive Scalable Texture Compression) Ч дл€ мобилок
		ASTC_4x4_UNORM,          // 4?4 блоки, 128 бит на блок (8 bpp)
		ASTC_4x4_SRGB,
		ASTC_5x4_UNORM,
		ASTC_5x4_SRGB,
		ASTC_5x5_UNORM,
		ASTC_5x5_SRGB,
		ASTC_6x5_UNORM,
		ASTC_6x5_SRGB,
		ASTC_6x6_UNORM,
		ASTC_6x6_SRGB,
		ASTC_8x5_UNORM,
		ASTC_8x5_SRGB,
		ASTC_8x6_UNORM,
		ASTC_8x6_SRGB,
		ASTC_8x8_UNORM,
		ASTC_8x8_SRGB,
		ASTC_10x5_UNORM,
		ASTC_10x5_SRGB,
		ASTC_10x6_UNORM,
		ASTC_10x6_SRGB,
		ASTC_10x8_UNORM,
		ASTC_10x8_SRGB,
		ASTC_10x10_UNORM,
		ASTC_10x10_SRGB,
		ASTC_12x10_UNORM,
		ASTC_12x10_SRGB,
		ASTC_12x12_UNORM,        // Ѕлоки 12?12, 128 бит (0.89 bpp) Ч максимальное сжатие
		ASTC_12x12_SRGB,

		// ETC2 / EAC (OpenGL ES 3.0, Vulkan на Android)
		ETC2_R8G8B8_UNORM,       // RGB, 4?4 блоки, 64 бита
		ETC2_R8G8B8_SRGB,
		ETC2_R8G8B8A1_UNORM,     // RGB + 1 бит альфы
		ETC2_R8G8B8A1_SRGB,
		ETC2_R8G8B8A8_UNORM,     // RGBA
		ETC2_R8G8B8A8_SRGB,
		EAC_R11_UNORM,           // 1 канал (аналог BC4)
		EAC_R11_SNORM,
		EAC_RG11_UNORM,          // 2 канала (аналог BC5)
		EAC_RG11_SNORM,

		// PVRTC (PowerVR Ч старые iOS устройства)
		PVRTC_RGB_2BPP_UNORM,    // 2 бита на пиксель
		PVRTC_RGB_2BPP_SRGB,
		PVRTC_RGBA_2BPP_UNORM,
		PVRTC_RGBA_2BPP_SRGB,
		PVRTC_RGB_4BPP_UNORM,    // 4 бита на пиксель
		PVRTC_RGB_4BPP_SRGB,
		PVRTC_RGBA_4BPP_UNORM,
		PVRTC_RGBA_4BPP_SRGB,

		// ==========================================
		// Depth / Stencil
		// ==========================================

		D16_UNORM,               // 16 бит глубины
		D24_UNORM_S8_UINT,       // 24 бита глубины + 8 бит трафарета (старый стандарт)
		D32_FLOAT,               // 32 бита float глубины (современный стандарт)
		D32_FLOAT_S8_UINT,       // 32 бита float глубины + 8 бит трафарета

		// “олько трафарет (редко)
		S8_UINT,
	};

	// TODO: Make flags in another way
	enum class TextureUsage : uint32_t {
		ShaderResource		= BIT(0),  // „тение в шейдере (samplers)
		RenderTarget		= BIT(1),  // ÷ветова€ цель рендера
		DepthStencil		= BIT(2),  // Ѕуфер глубины/трафарета
		UnorderedAccess		= BIT(3),  // RW в compute шейдере
		TransferSrc			= BIT(4),  // ƒл€ копировани€
		TransferDst			= BIT(5),  // ƒл€ копировани€
		CubeMap				= BIT(6),  // ѕодсказка дл€ оптимизаций
	};

	enum class TextureType : uint8_t {
		Texture1D,
		Texture2D,
		Texture3D,
		TextureCube,
		Texture1DArray,
		Texture2DArray,
		TextureCubeArray,
	};

	enum class PrimitiveType : uint8_t {
		Points,
		Lines, LineStrip, LineLoop,
		Triangles, TriangleStrip, TriangleFan
	};

	enum class IndexFormat : uint8_t {
		UInt16,
		UInt32
	};

	// TODO: Make flags in another way
	enum class BufferUsage : uint32_t {
		Vertex		= BIT(0),
		Index		= BIT(1),
		Uniform		= BIT(2),
		Storage		= BIT(3),
		Indirect	= BIT(4),
	};

	enum class MemoryType : uint8_t {
		GpuOnly,     // Vertex/Index buffers, textures
		CpuToGpu,    // Uniform buffers, dynamic data
		GpuToCpu,    // Readback
	};
}
