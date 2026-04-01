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

		virtual void AddVertexBuffer(const Ref<CHEngine::IVertexBuffer>& vertexBuffer) override;
		virtual void SetIndexBuffer(const Ref<CHEngine::IIndexBuffer>& indexBuffer) override;

		virtual const CHEngine::Vector<Ref<CHEngine::IVertexBuffer>>& GetVertexBuffers() const override;
		virtual const Ref<CHEngine::IIndexBuffer>& GetIndexBuffer() const override;
	private:
		uint32_t m_RendererID;
		uint32_t m_VertexBufferIndex = 0;
		CHEngine::Vector<Ref<CHEngine::IVertexBuffer>> m_VertexBuffers;
		Ref<CHEngine::IIndexBuffer> m_IndexBuffer;
	};

}