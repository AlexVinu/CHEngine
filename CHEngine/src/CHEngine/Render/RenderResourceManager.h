#pragma once

#include <Core.h>
#include <Memory/Handle.h>
#include <Memory/HandlePool.h>
#include <CheStl/String.h>
#include <CheStl/Vector.h>
#include <CheStl/MemoryTypes.h>

#include "Render/IRenderFactory.h"
#include "Render/IFramebuffer.h"
#include "Render/UniformBlocks.h"
#include "RenderData.h"
#include "CHEngine/Utils/FileWatcher.h"

#include <memory>
#include <string>

namespace CHEngine {

	struct ShaderTag {};
	struct VertexArrayTag {};
	struct TextureTag {};
	struct FramebufferTag {};

	using ShaderHandle      = Handle<ShaderTag>;
	using VertexArrayHandle = Handle<VertexArrayTag>;
	using TextureHandle     = Handle<TextureTag>;
	using FramebufferHandle = Handle<FramebufferTag>;

	// Metadata stored for every named shader (enables hot-reload and manager UI).
	// One Slang file → vertex + fragment entry points.
	struct ShaderEntry
	{
		String       name;
		String       slangPath;
		String       vertEntry;
		String       fragEntry;
		ShaderHandle handle;
		bool         valid = false;
	};

	class CHENGINE_API RenderResourceManager
	{
	public:
		RenderResourceManager() = default;

		void Init(IRenderFactory* factory);

		// Unnamed — creates a shader without registering it in the shader list.
		// sourcePath (optional) is forwarded to the backend so Slang can resolve
		// `import` directives relative to the file's directory.
		ShaderHandle CreateShader(const String& slangSource,
		                          const String& vertEntry  = String("vertMain"),
		                          const String& fragEntry  = String("fragMain"),
		                          const String& sourcePath = String());

		// Creates a shader from a single .slang file and registers it for
		// hot-reload / manager UI.
		ShaderHandle CreateShaderFromFile(const String& name,
		                                  const String& slangPath,
		                                  const String& vertEntry = String("vertMain"),
		                                  const String& fragEntry = String("fragMain"));

		VertexArrayHandle CreateVertexArray();
		// Creates the single active IRenderApi, calls Init(init_info). Idempotent.
		// Returns nullptr on failure.
		IRenderer* InitRenderer(const RendererInitInfo& init_info);
		IRenderApi* GetRenderAPI() const;
		IRenderer* GetRenderer() const;

		// Upload raw pixel data as a GPU texture.
		// channels: 1=R, 2=RG, 3=RGB, 4=RGBA.
		TextureHandle CreateTexture(const uint8_t* data, uint32_t width,
		                            uint32_t height, uint32_t channels);

		// Загружает изображение с диска (PNG/JPG/TGA/BMP) через stb_image.
		// Возвращает Invalid handle при ошибке.
		TextureHandle CreateTextureFromFile(const std::string& path);

		// Create an offscreen framebuffer (FBO) for rendering into a texture.
		FramebufferHandle CreateFramebuffer(uint32_t width, uint32_t height);

		Ref<IVertexBuffer> CreateVertexBuffer(float* vertices, uint32_t size);
		Ref<IIndexBuffer>  CreateIndexBuffer(uint32_t* indices, uint32_t count);

		IShader*      Get(ShaderHandle h) const;
		IVertexArray* Get(VertexArrayHandle h) const;
		ITexture*     Get(TextureHandle h) const;
		IFramebuffer* Get(FramebufferHandle h) const;

		// Re-reads shader files from disk and recompiles in-place.
		// Returns true on success; the old GPU program remains on failure.
		bool ReloadShader(ShaderHandle h);

		// Опросить FileWatcher и перезагрузить изменившиеся шейдеры.
		// Вызывать раз в 0.5–1 сек (не каждый кадр).
		void PollShaders();

		const Vector<ShaderEntry>& GetShaderEntries() const;

		// Per-frame camera for Submit(Shader, VAO, transform). Invalidated at BeginFrame.
		void SetSceneCamera(const UBOCamera& camera);
		const UBOCamera& GetSceneCamera() const { return m_SceneCamera; }
		bool HasSceneCamera() const { return m_SceneCameraValid; }
		void InvalidateSceneCameraForFrame();

		void DestroyShader(ShaderHandle h);
		void DestroyVertexArray(VertexArrayHandle h);
		void DestroyRenderAPI();
		void DestroyRenderer();
		void DestroyTexture(TextureHandle h);
		void DestroyFramebuffer(FramebufferHandle h);

		void Shutdown();

		static void SetDefaultMeshShader(ShaderHandle h);
		static ShaderHandle GetDefaultMeshShader();

	private:
		IRenderApi* InitRenderAPI(const RendererInitInfo& init_info);
		static String ReadTextFile(const String& path);

		IRenderFactory* m_Factory = nullptr;

		FileWatcher m_ShaderWatcher;

		HandlePool<IShader,      ShaderTag>       m_Shaders;
		HandlePool<IVertexArray, VertexArrayTag>  m_VertexArrays;
		HandlePool<ITexture,     TextureTag>      m_Textures;
		HandlePool<IFramebuffer, FramebufferTag>  m_Framebuffers;

		IRenderApi* m_RenderApi = nullptr;
		IRenderer* m_Renderer = nullptr;

		Vector<ShaderEntry> m_ShaderEntries;

		UBOCamera m_SceneCamera{};
		bool m_SceneCameraValid = false;

		static ShaderHandle s_DefaultMeshShader;
	};

}
