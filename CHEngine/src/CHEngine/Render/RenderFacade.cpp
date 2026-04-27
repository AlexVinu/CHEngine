#include "chepch.h"

#include "RenderFacade.h"

#include "CHEngine/Application.h"
#include "Render/IRenderer.h"

namespace CHEngine
{
	RenderResourceManager* GetRenderResources()
	{
		CHE_ASSERT(CHEngine::Application::Get().GetRenderResources(), "RENDER RESOURCES NOT INITIALIZED");
		return CHEngine::Application::Get().GetRenderResources();
	}

	IRenderApi* GetActiveRenderAPI()
	{
		auto* res = GetRenderResources();
		IRenderApi* api = res->GetRenderAPI();
		CHE_ASSERT(api, "RENDER API NOT INITIALIZED");
		return api;
	}

	IRenderer* GetActiveRenderer()
	{
		auto* res = GetRenderResources();
		IRenderer* renderer = res->GetRenderer();
		CHE_ASSERT(renderer, "RENDER API NOT INITIALIZED");
		return renderer;
	}
}

namespace CHEngine
{
	void RenderFacade::InitRenderer(const RendererInitInfo& init_info)
	{
		CHE_ASSERT(CHEngine::Application::Get().GetRenderResources(), "RENDER RESOURCES NOT INITIALIZED");
		GetRenderResources()->InitRenderer(init_info);
	}
	void RenderFacade::SetClearColor(float r, float g, float b, float a)
	{
		IRenderApi* api = GetActiveRenderAPI();
		api->SetClearColor(r, g, b, a);
	}

	void RenderFacade::Clear()
	{
		IRenderApi* api = GetActiveRenderAPI();
		api->Clear();
	}

	void RenderFacade::SetViewport(uint32_t width, uint32_t height)
	{
		IRenderApi* api = GetActiveRenderAPI();
		api->SetViewport(width, height);
	}

	void RenderFacade::SetBlend(bool enable)
	{
		IRenderApi* api = GetActiveRenderAPI();
		api->SetBlend(enable);
	}

	void RenderFacade::SetDepthWrite(bool enable)
	{
		IRenderApi* api = GetActiveRenderAPI();
		api->SetDepthWrite(enable);
	}

	void RenderFacade::BeginFrame()
	{
		GetRenderResources()->InvalidateSceneCameraForFrame();
		GetActiveRenderAPI()->BeginFrame();
	}

	void RenderFacade::EndFrame()
	{
		GetActiveRenderAPI()->EndFrame();
	}

	void RenderFacade::BeginScene()
	{
		IRenderer* renderer = GetActiveRenderer();
		CHE_ASSERT(renderer, "RENDERER NOT INITIALIZED");
		renderer->BeginScene();
	}

	void RenderFacade::EndScene()
	{
		IRenderer* renderer = GetActiveRenderer();
		CHE_ASSERT(renderer, "RENDERER NOT INITIALIZED");
		renderer->EndScene();
	}

	void RenderFacade::SetSceneCamera(const UBOCamera& camera)
	{
		GetRenderResources()->SetSceneCamera(camera);
	}

	void RenderFacade::Submit(ShaderHandle shader, VertexArrayHandle vertexArray, const glm::mat4& transform)
	{
		CHE_ASSERT(shader.IsValid(), "SHADER HANDLE IS INVALID");
		CHE_ASSERT(vertexArray.IsValid(), "VERTEX ARRAY HANDLE IS INVALID");

		auto* res = GetRenderResources();
		CHE_ASSERT(res->HasSceneCamera(), "SetSceneCamera must be called before Submit(shader, ...)");

		IShader* sh = res->Get(shader);
		IVertexArray* vao = res->Get(vertexArray);
		CHE_ASSERT(sh, "SHADER NOT FOUND");
		CHE_ASSERT(vao, "VERTEX ARRAY NOT FOUND");

		IRenderer* renderer = GetActiveRenderer();
		CHE_ASSERT(renderer, "RENDERER NOT INITIALIZED");
		renderer->Submit(sh, vao, transform, res->GetSceneCamera());
	}

	void RenderFacade::Submit(VertexArrayHandle vertexArray, const glm::mat4& transform)
	{
		CHE_ASSERT(vertexArray.IsValid(), "VERTEX ARRAY HANDLE IS INVALID");

		auto* res = GetRenderResources();
		IRenderer* renderer = GetActiveRenderer();
		CHE_ASSERT(renderer, "RENDERER NOT INITIALIZED");

		IVertexArray* vao = res->Get(vertexArray);
		CHE_ASSERT(vao, "VERTEX ARRAY NOT FOUND");

		renderer->Submit(vao, transform);
	}

	void RenderFacade::SubmitLines(VertexArrayHandle vertexArray, const glm::mat4& transform)
	{
		CHE_ASSERT(vertexArray.IsValid(), "VERTEX ARRAY HANDLE IS INVALID");

		auto* res = GetRenderResources();
		IRenderer* renderer = GetActiveRenderer();
		CHE_ASSERT(renderer, "RENDERER NOT INITIALIZED");

		IVertexArray* vao = res->Get(vertexArray);
		CHE_ASSERT(vao, "VERTEX ARRAY NOT FOUND");

		renderer->SubmitLines(vao, transform);
	}

	void RenderFacade::DrawIndexed(VertexArrayHandle vertexArray)
	{
		Submit(vertexArray, glm::mat4(1.0f));
	}

	void RenderFacade::DrawLines(VertexArrayHandle vertexArray)
	{
		SubmitLines(vertexArray, glm::mat4(1.0f));
	}

	ShaderHandle RenderFacade::CreateShader(const String& slangSource,
	                                        const String& vertEntry,
	                                        const String& fragEntry)
	{
		return GetRenderResources()->CreateShader(slangSource, vertEntry, fragEntry);
	}

	ShaderHandle RenderFacade::CreateShaderFromFile(const String& name,
	                                                const String& slangPath,
	                                                const String& vertEntry,
	                                                const String& fragEntry)
	{
		return GetRenderResources()->CreateShaderFromFile(name, slangPath, vertEntry, fragEntry);
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

	Ref<IVertexBuffer> RenderFacade::CreateVertexBuffer(float* vertices, uint32_t size)
	{
		return GetRenderResources()->CreateVertexBuffer(vertices, size);
	}

	Ref<IIndexBuffer> RenderFacade::CreateIndexBuffer(uint32_t* indices, uint32_t count)
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

	IRenderApi* RenderFacade::GetRenderAPI()
	{
		return GetActiveRenderAPI();
	}

	void RenderFacade::SetDefaultMeshShader(ShaderHandle h)
	{
		GetRenderResources()->SetDefaultMeshShader(h);
	}

	ShaderHandle RenderFacade::GetDefaultMeshShader()
	{
		return GetRenderResources()->GetDefaultMeshShader();
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
