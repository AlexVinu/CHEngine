#pragma once

#include "Core.h"
#include "RenderResourceManager.h"

#include <glm/mat4x4.hpp>

namespace CHEngine
{
	class CHENGINE_API RenderFacade
	{
	public:
		static void InitRenderer(const RendererInitInfo& init_info);
		// Frame commands
		static void SetClearColor(float r, float g, float b, float a);
		static void Clear();
		static void SetViewport(uint32_t width, uint32_t height);
		static void SetBlend(bool enable);
		static void SetDepthWrite(bool enable);
		static void BeginFrame();
		static void EndFrame();

		static void BeginScene();
		static void EndScene();
		static void SetSceneCamera(const UBOCamera& camera);
		static void Submit(ShaderHandle shader, VertexArrayHandle vertexArray, const glm::mat4& transform);
		static void Submit(VertexArrayHandle vertexArray, const glm::mat4& transform);
		static void SubmitLines(VertexArrayHandle vertexArray, const glm::mat4& transform);

		static void DrawIndexed(VertexArrayHandle vertexArray);
		static void DrawLines(VertexArrayHandle vertexArray);

		// Resource creation
		static ShaderHandle CreateShader(const String& slangSource,
		                                 const String& vertEntry = String("vertMain"),
		                                 const String& fragEntry = String("fragMain"));
		static ShaderHandle CreateShaderFromFile(const String& name,
		                                         const String& slangPath,
		                                         const String& vertEntry = String("vertMain"),
		                                         const String& fragEntry = String("fragMain"));
		static VertexArrayHandle CreateVertexArray();
		static TextureHandle CreateTexture(const uint8_t* data, uint32_t width,
		                                    uint32_t height, uint32_t channels);
		static TextureHandle CreateTextureFromFile(const std::string& path);
		static FramebufferHandle CreateFramebuffer(uint32_t width, uint32_t height);
		static Ref<IVertexBuffer> CreateVertexBuffer(float* vertices, uint32_t size);
		static Ref<IIndexBuffer>  CreateIndexBuffer(uint32_t* indices, uint32_t count);

		// Resource access
		static IShader*       GetShader(ShaderHandle h);
		static IVertexArray* GetVertexArray(VertexArrayHandle h);
		static ITexture*     GetTexture(TextureHandle h);
		static IFramebuffer* GetFramebuffer(FramebufferHandle h);
		static IRenderApi*   GetRenderAPI();

		static void SetDefaultMeshShader(ShaderHandle h);
		static ShaderHandle GetDefaultMeshShader();

		// Resource destroy
		static void DestroyShader(ShaderHandle h);
		static void DestroyVertexArray(VertexArrayHandle h);
		static void DestroyTexture(TextureHandle h);
		static void DestroyFramebuffer(FramebufferHandle h);

	private:
		RenderFacade() = delete;
		~RenderFacade() = delete;
		RenderFacade(const RenderFacade&) = delete;
		RenderFacade& operator=(const RenderFacade&) = delete;
		RenderFacade(RenderFacade&&) = delete;
		RenderFacade& operator=(RenderFacade&&) = delete;
	};
}