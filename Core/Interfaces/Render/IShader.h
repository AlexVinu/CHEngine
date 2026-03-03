#pragma once

#include<Core.h>

namespace CHEngine {

	class IShader
	{
	public:
		virtual ~IShader() = default;

		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;
	protected:
		uint32_t m_RendererID;
	};
}
