#pragma once

#include <cstdint>
#include "Memory/Handle.h"

namespace CHEngine {

    const uint32_t BackbufferId = 0xFFFFFFFFu; 

	class IRGPassBuilder;
	class RenderContext;

	struct RGResourceTag {};
	using RGResourceHandle = Handle<RGResourceTag>;

	enum class ResourceType : uint8_t {
		NONE = 0,
		Buffer = 1,
		Texture1D = 2, Texture2D = 3, Texture3D = 4, TextureCube = 5,
		Count = 6
	};

	enum class ResourceUsageFlags : uint32_t {
		NONE			= 0,
		RenderTarget	= BIT(0),
		DepthStencil	= BIT(1),
		ShaderResource  = BIT(2),
		UnorderedAccess = BIT(3),
	};

	// Texture formats supported by the render graph.
	enum class RGFormat
	{
		NONE			= 0,
		R8				= 1,
		RG8				= 2,
		RGBA8			= 3,
		RGBA16F		    = 4,
		Depth24Stencil8 = 5,
		Depth32F		= 6,
	};

	struct RGResourceDesc {
		ResourceType Type = ResourceType::NONE;

		RGFormat Format = RGFormat::NONE;

		uint32_t Width = 0, Height = 0, DepthOrLayers = 1;
		uint16_t MipLevels = 1;
		uint8_t SampleCount = 1;

		ResourceUsageFlags Usage = ResourceUsageFlags::NONE;

		bool IsCpuAccessible = false;

		std::string Name;

		bool operator==(const RGResourceDesc& other) const {
			return memcmp(this, &other, sizeof(RGResourceDesc)) == 0;
		}

		uint64_t Hash() const {
			return std::hash<std::string_view>{}(
				std::string_view(reinterpret_cast<const char*>(this), sizeof(*this))
				);
		}
	};

} // namespace CHEngine
