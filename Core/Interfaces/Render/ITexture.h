#pragma once

#include <Core.h>
#include <cstdint>

namespace CHEngine {

	class ITexture
	{
	public:
		virtual ~ITexture() = default;

		virtual void Bind(uint32_t slot = 0) const = 0;
		virtual void Unbind() const = 0;

		virtual uint32_t GetWidth()  const = 0;
		virtual uint32_t GetHeight() const = 0;
	};

}
