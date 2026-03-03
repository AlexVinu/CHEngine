#pragma once

#include <Core.h>

#include <memory>
#include "IBuffer.h"

namespace CHEngine {

	class IVertexArray
	{
	public:
		virtual ~IVertexArray() = default;

		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		virtual void AddVertexBuffer(const std::shared_ptr<IVertexBuffer>& vertexBuffer) = 0;
		virtual void SetIndexBuffer(const std::shared_ptr<IIndexBuffer>& indexBuffer) = 0;

		virtual const CHEngine::Vector<std::shared_ptr<IVertexBuffer>>& GetVertexBuffers() const = 0;
		virtual const std::shared_ptr<IIndexBuffer>& GetIndexBuffer() const = 0;
	};
}
