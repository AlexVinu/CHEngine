#include "chepch.h"

#include "RenderFacade.h"

#include "CHEngine/Application.h"

namespace CHEngine
{
	RenderResourceManager* GetRenderResources()
	{
		CHE_ASSERT(CHEngine::Application::Get().GetRenderResources(), "RENDER RESOURCES NOT INITIALIZED");
		return CHEngine::Application::Get().GetRenderResources();
	}

	RendererAPI* GetActiveRenderAPI()
	{
		auto* res = GetRenderResources();
		RendererAPI* api = res->GetRenderAPI();
		CHE_ASSERT(api, "RENDER API NOT INITIALIZED");
		return api;
	}
}

namespace CHEngine
{
	void RenderFacade::InitAPI()
	{
		CHE_ASSERT(CHEngine::Application::Get().GetRenderResources(), "RENDER RESOURCES NOT INITIALIZED");
		auto* res = GetRenderResources();
		RendererAPI* api = res->GetRenderAPI();
		if (!api)
			api = res->InitRenderAPI();
	}
	void RenderFacade::SetClearColor(float r, float g, float b, float a)
	{
		RendererAPI* api = GetActiveRenderAPI();
		api->SetClearColor(r, g, b, a);
	}

	void RenderFacade::Clear()
	{
		RendererAPI* api = GetActiveRenderAPI();
		api->Clear();
	}

	void RenderFacade::SetViewport(uint32_t width, uint32_t height)
	{
		RendererAPI* api = GetActiveRenderAPI();
		api->SetViewport(width, height);
	}

	void RenderFacade::SetBlend(bool enable)
	{
		RendererAPI* api = GetActiveRenderAPI();
		api->SetBlend(enable);
	}

	void RenderFacade::SetDepthWrite(bool enable)
	{
		RendererAPI* api = GetActiveRenderAPI();
		api->SetDepthWrite(enable);
	}

	void RenderFacade::DrawIndexed(VertexArrayHandle vertexArray)
	{
		CHE_ASSERT(vertexArray.IsValid(), "VERTEX ARRAY HANDLE IS INVALID");

		auto* res = GetRenderResources();
		RendererAPI* api = GetActiveRenderAPI();

		IVertexArray* vao = res->Get(vertexArray);
		CHE_ASSERT(vao, "VERTEX ARRAY NOT FOUND");

		api->DrawIndexed(vao);
	}

	void RenderFacade::DrawLines(VertexArrayHandle vertexArray)
	{
		CHE_ASSERT(vertexArray.IsValid(), "VERTEX ARRAY HANDLE IS INVALID");

		auto* res = GetRenderResources();
		RendererAPI* api = GetActiveRenderAPI();

		IVertexArray* vao = res->Get(vertexArray);
		CHE_ASSERT(vao, "VERTEX ARRAY NOT FOUND");

		api->DrawLines(vao);
	}

	ShaderHandle RenderFacade::CreateShader(const String& vertexSrc, const String& fragmentSrc)
	{
		return GetRenderResources()->CreateShader(vertexSrc, fragmentSrc);
	}

	ShaderHandle RenderFacade::CreateShaderFromFile(const String& name,
	                                                  const String& vertexPath,
	                                                  const String& fragmentPath)
	{
		return GetRenderResources()->CreateShaderFromFile(name, vertexPath, fragmentPath);
	}

	VertexArrayHandle RenderFacade::CreateVertexArray()
	{
		return GetRenderResources()->CreateVertexArray();
	}

	TextureHandle RenderFacade::CreateTexture(const uint8_t* data, uint32_t width,
	                                           uint32_t height, uint32_t channels)
	{
		return GetRenderResources()->CreateTexture(data, width, height, channels);
	}

	TextureHandle RenderFacade::CreateTextureFromFile(const std::string& path)
	{
		return GetRenderResources()->CreateTextureFromFile(path);
	}

	FramebufferHandle RenderFacade::CreateFramebuffer(uint32_t width, uint32_t height)
	{
		return GetRenderResources()->CreateFramebuffer(width, height);
	}

	std::shared_ptr<IVertexBuffer> RenderFacade::CreateVertexBuffer(float* vertices, uint32_t size)
	{
		return GetRenderResources()->CreateVertexBuffer(vertices, size);
	}

	std::shared_ptr<IIndexBuffer> RenderFacade::CreateIndexBuffer(uint32_t* indices, uint32_t count)
	{
		return GetRenderResources()->CreateIndexBuffer(indices, count);
	}

	IShader* RenderFacade::GetShader(ShaderHandle h)
	{
		return GetRenderResources()->Get(h);
	}

	IVertexArray* RenderFacade::GetVertexArray(VertexArrayHandle h)
	{
		return GetRenderResources()->Get(h);
	}

	ITexture* RenderFacade::GetTexture(TextureHandle h)
	{
		return GetRenderResources()->Get(h);
	}

	IFramebuffer* RenderFacade::GetFramebuffer(FramebufferHandle h)
	{
		return GetRenderResources()->Get(h);
	}

	RendererAPI* RenderFacade::GetRenderAPI()
	{
		return GetActiveRenderAPI();
	}

	void RenderFacade::DestroyShader(ShaderHandle h)
	{
		GetRenderResources()->DestroyShader(h);
	}

	void RenderFacade::DestroyVertexArray(VertexArrayHandle h)
	{
		GetRenderResources()->DestroyVertexArray(h);
	}

	void RenderFacade::DestroyTexture(TextureHandle h)
	{
		GetRenderResources()->DestroyTexture(h);
	}

	void RenderFacade::DestroyFramebuffer(FramebufferHandle h)
	{
		GetRenderResources()->DestroyFramebuffer(h);
	}

}