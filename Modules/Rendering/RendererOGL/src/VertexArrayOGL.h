#pragma once

#include "Render/IVertexArray.h"

namespace CHModules {

	class VertexArrayOGL : public CHEngine::IVertexArray
	{
	public:
		VertexArrayOGL();
		virtual ~VertexArrayOGL();

		virtual void Bind() const override;
		virtual void Unbind() const override;

		virtual void AddVertexBuffer(const std::shared_ptr<CHEngine::IVertexBuffer>& vertexBuffer) override;
		virtual void SetIndexBuffer(const std::shared_ptr<CHEngine::IIndexBuffer>& indexBuffer) override;

		virtual const CHEngine::Vector<std::shared_ptr<CHEngine::IVertexBuffer>>& GetVertexBuffers() const override;
		virtual const std::shared_ptr<CHEngine::IIndexBuffer>& GetIndexBuffer() const override;
	private:
		uint32_t m_RendererID;
		uint32_t m_VertexBufferIndex = 0;
		CHEngine::Vector<std::shared_ptr<CHEngine::IVertexBuffer>> m_VertexBuffers;
		std::shared_ptr<CHEngine::IIndexBuffer> m_IndexBuffer;
	};

}