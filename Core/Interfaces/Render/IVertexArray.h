#pragma once

#include <Core.h>

#include "CheStl/MemoryTypes.h"

#include "IBuffer.h"

namespace CHEngine {

	class IVertexArray
	{
	public:
		virtual ~IVertexArray() = default;

		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		virtual void AddVertexBuffer(const Ref<IVertexBuffer>& vertexBuffer) = 0;
		virtual void SetIndexBuffer(const Ref<IIndexBuffer>& indexBuffer) = 0;

		virtual const CHEngine::Vector<Ref<IVertexBuffer>>& GetVertexBuffers() const = 0;
		virtual const Ref<IIndexBuffer>& GetIndexBuffer() const = 0;
	};
}
