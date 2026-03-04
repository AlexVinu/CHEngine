#include "chepch.h"
#include "RenderResourceManager.h"

#include <fstream>
#include <sstream>

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

	String RenderResourceManager::ReadTextFile(const String& path)
	{
		std::ifstream file(path.c_str());
		if (!file.is_open())
		{
			CHE_CORE_ERROR("RenderResourceManager: cannot open file '{0}'", path.c_str());
			return {};
		}
		std::ostringstream ss;
		ss << file.rdbuf();
		return String(ss.str().c_str());
	}

	ShaderHandle RenderResourceManager::CreateShaderFromFile(const String& vertexPath, const String& fragmentPath)
	{
		String vertexSrc   = ReadTextFile(vertexPath);
		String fragmentSrc = ReadTextFile(fragmentPath);

		if (vertexSrc.size() == 0 || fragmentSrc.size() == 0)
		{
			CHE_CORE_ERROR("RenderResourceManager: shader file(s) could not be read, shader not created");
			return ShaderHandle::Invalid();
		}

		CHE_CORE_INFO("Loaded shader: '{0}' + '{1}'", vertexPath.c_str(), fragmentPath.c_str());
		return CreateShader(vertexSrc, fragmentSrc);
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
