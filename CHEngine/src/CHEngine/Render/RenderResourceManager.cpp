#include "chepch.h"
#include "RenderResourceManager.h"

namespace CHEngine {

	void RenderResourceManager::Init(IRenderFactory* factory)
	{
		CHE_CORE_ASSERT(factory, "RenderResourceManager::Init — factory is null");
		m_Factory = factory;

		m_Shaders = HandlePool<IShader, ShaderTag>(
			[this](IShader* ptr) { m_Factory->Delete(ptr); }
		);

		m_VertexArrays = HandlePool<IVertexArray, VertexArrayTag>(
			[this](IVertexArray* ptr) { m_Factory->Delete(ptr); }
		);

		m_RenderApis = HandlePool<RendererAPI, RenderAPITag>(
			[this](RendererAPI* ptr) { m_Factory->Delete(ptr); }
		);
	}

	ShaderHandle RenderResourceManager::CreateShader(const String& vertexSrc, const String& fragmentSrc)
	{
		IShader* shader = m_Factory->CreateShader(vertexSrc, fragmentSrc);
		if (!shader)
		{
			CHE_CORE_ERROR("RenderResourceManager: failed to create shader");
			return ShaderHandle::Invalid();
		}
		return m_Shaders.Add(shader);
	}

	VertexArrayHandle RenderResourceManager::CreateVertexArray()
	{
		IVertexArray* vao = m_Factory->CreateVertexArray();
		if (!vao)
		{
			CHE_CORE_ERROR("RenderResourceManager: failed to create vertex array");
			return VertexArrayHandle::Invalid();
		}
		return m_VertexArrays.Add(vao);
	}

	RenderAPIHandle RenderResourceManager::CreateRenderAPI()
	{
		RendererAPI* api = m_Factory->CreateRenderAPI();
		if (!api)
		{
			CHE_CORE_ERROR("RenderResourceManager: failed to create render API");
			return RenderAPIHandle::Invalid();
		}
		return m_RenderApis.Add(api);
	}

	std::shared_ptr<IVertexBuffer> RenderResourceManager::CreateVertexBuffer(float* vertices, uint32_t size)
	{
		IVertexBuffer* vb = m_Factory->CreateVertexBuffer(vertices, size);
		if (!vb)
		{
			CHE_CORE_ERROR("RenderResourceManager: failed to create vertex buffer");
			return nullptr;
		}
		return std::shared_ptr<IVertexBuffer>(vb,
			[this](IVertexBuffer* ptr) { m_Factory->Delete(ptr); }
		);
	}

	std::shared_ptr<IIndexBuffer> RenderResourceManager::CreateIndexBuffer(uint32_t* indices, uint32_t count)
	{
		IIndexBuffer* ib = m_Factory->CreateIndexBuffer(indices, count);
		if (!ib)
		{
			CHE_CORE_ERROR("RenderResourceManager: failed to create index buffer");
			return nullptr;
		}
		return std::shared_ptr<IIndexBuffer>(ib,
			[this](IIndexBuffer* ptr) { m_Factory->Delete(ptr); }
		);
	}

	IShader* RenderResourceManager::Get(ShaderHandle h) const
	{
		return m_Shaders.Get(h);
	}

	IVertexArray* RenderResourceManager::Get(VertexArrayHandle h) const
	{
		return m_VertexArrays.Get(h);
	}

	RendererAPI* RenderResourceManager::Get(RenderAPIHandle h) const
	{
		return m_RenderApis.Get(h);
	}

	void RenderResourceManager::DestroyShader(ShaderHandle h)
	{
		m_Shaders.Remove(h);
	}

	void RenderResourceManager::DestroyVertexArray(VertexArrayHandle h)
	{
		m_VertexArrays.Remove(h);
	}

	void RenderResourceManager::DestroyRenderAPI(RenderAPIHandle h)
	{
		m_RenderApis.Remove(h);
	}

	void RenderResourceManager::Shutdown()
	{
		m_Shaders.Clear();
		m_VertexArrays.Clear();
		m_RenderApis.Clear();
	}

}
