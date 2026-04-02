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

		virtual void AddVertexBuffer(const CHEngine::Ref<CHEngine::IVertexBuffer>& vertexBuffer) override;
		virtual void SetIndexBuffer(const CHEngine::Ref<CHEngine::IIndexBuffer>& indexBuffer) override;

		virtual const CHEngine::Vector<CHEngine::Ref<CHEngine::IVertexBuffer>>& GetVertexBuffers() const override;
		virtual const CHEngine::Ref<CHEngine::IIndexBuffer>& GetIndexBuffer() const override;
	private:
		uint32_t m_RendererID;
		uint32_t m_VertexBufferIndex = 0;
		CHEngine::Vector<CHEngine::Ref<CHEngine::IVertexBuffer>> m_VertexBuffers;
		CHEngine::Ref<CHEngine::IIndexBuffer> m_IndexBuffer;
	};

}