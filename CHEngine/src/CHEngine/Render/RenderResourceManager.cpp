#include "chepch.h"
#include "RenderResourceManager.h"

#include "FileSystem/FileSystem.h"

#include <climits>
#include <filesystem>
#include <stb_image.h>

namespace CHEngine {

	ShaderHandle RenderResourceManager::s_DefaultMeshShader{};

	void RenderResourceManager::SetDefaultMeshShader(ShaderHandle h)
	{
		s_DefaultMeshShader = h;
	}

	ShaderHandle RenderResourceManager::GetDefaultMeshShader()
	{
		return s_DefaultMeshShader;
	}

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

		m_Textures = HandlePool<ITexture, TextureTag>(
			[this](ITexture* ptr) { m_Factory->Delete(ptr); }
		);

		m_Framebuffers = HandlePool<IFramebuffer, FramebufferTag>(
			[this](IFramebuffer* ptr) { m_Factory->Delete(ptr); }
		);
	}

	String RenderResourceManager::ReadTextFile(const String& path)
	{
		const std::filesystem::path fsPath(path.c_str());
		if (!FileSystem::Exists(fsPath))
		{
			CHE_CORE_ERROR("RenderResourceManager: cannot open file '{0}'", path.c_str());
			return {};
		}
		return String(FileSystem::ReadFileText(fsPath).c_str());
	}

	ShaderHandle RenderResourceManager::CreateShaderFromFile(const String& name,
	                                                         const String& slangPath,
	                                                         const String& vertEntry,
	                                                         const String& fragEntry)
	{
		String slangSource = ReadTextFile(slangPath);

		bool valid = (slangSource.size() > 0);

		// Pass an absolute path so Slang resolves `import` directives relative
		// to the .slang file's directory regardless of the current working dir.
		String absPath;
		{
			std::error_code ec;
			auto fs_abs = std::filesystem::absolute(std::filesystem::path(slangPath.c_str()), ec);
			absPath = ec ? slangPath : String(fs_abs.string().c_str());
		}

		ShaderHandle handle = ShaderHandle::Invalid();
		if (valid)
		{
			handle = CreateShader(slangSource, vertEntry, fragEntry, absPath);
			valid  = handle.IsValid();
		}

		if (valid)
			CHE_CORE_INFO("Loaded shader '{0}': '{1}'", name.c_str(), slangPath.c_str());
		else
			CHE_CORE_ERROR("RenderResourceManager: failed to load shader '{0}' from '{1}'",
			               name.c_str(), slangPath.c_str());

		ShaderEntry entry;
		entry.name      = name;
		entry.slangPath = slangPath;
		entry.vertEntry = vertEntry;
		entry.fragEntry = fragEntry;
		entry.handle    = handle;
		entry.valid     = valid;
		m_ShaderEntries.push_back(std::move(entry));

		// Автоматически регистрируем шейдер для hot reload — один файл .slang
		if (valid) {
			auto reloadFn = [this, handle](const std::filesystem::path&) { ReloadShader(handle); };
			m_ShaderWatcher.Watch(std::filesystem::path(slangPath.c_str()), reloadFn);
		}

		return handle;
	}

	ShaderHandle RenderResourceManager::CreateShader(const String& slangSource,
	                                                 const String& vertEntry,
	                                                 const String& fragEntry,
	                                                 const String& sourcePath)
	{
		IShader* shader = m_Factory->CreateShader(slangSource, vertEntry, fragEntry, sourcePath);
		if (!shader)
		{
			CHE_CORE_ERROR("RenderResourceManager: failed to create shader");
			return ShaderHandle::Invalid();
		}
		return m_Shaders.Add(shader);
	}

	bool RenderResourceManager::ReloadShader(ShaderHandle h)
	{
		ShaderEntry* entry = nullptr;
		for (ShaderEntry& e : m_ShaderEntries)
		{
			if (e.handle == h)
			{
				entry = &e;
				break;
			}
		}

		if (!entry)
		{
			CHE_CORE_ERROR("ReloadShader: no entry found for this handle");
			return false;
		}

		String slangSource = ReadTextFile(entry->slangPath);

		if (slangSource.size() == 0)
		{
			CHE_CORE_ERROR("ReloadShader: could not read '{0}' for '{1}'",
			               entry->slangPath.c_str(), entry->name.c_str());
			entry->valid = false;
			return false;
		}

		IShader* shader = m_Shaders.Get(h);
		if (!shader)
		{
			CHE_CORE_ERROR("ReloadShader: shader handle is no longer valid");
			entry->valid = false;
			return false;
		}

		String absPath;
		{
			std::error_code ec;
			auto fs_abs = std::filesystem::absolute(std::filesystem::path(entry->slangPath.c_str()), ec);
			absPath = ec ? entry->slangPath : String(fs_abs.string().c_str());
		}

		bool success = shader->Reload(slangSource, entry->vertEntry, entry->fragEntry, absPath);
		entry->valid = success;

		if (success)
			CHE_CORE_INFO("Reloaded shader '{0}'", entry->name.c_str());
		else
			CHE_CORE_ERROR("ReloadShader: compilation failed for '{0}' — keeping old program", entry->name.c_str());

		return success;
	}

	void RenderResourceManager::PollShaders()
	{
		m_ShaderWatcher.Poll();
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

	IRenderApi* RenderResourceManager::InitRenderAPI(const RendererInitInfo& init_info)
	{
		CHE_CORE_ASSERT(m_Factory, "RenderResourceManager::InitRenderAPI — factory is null (Init not called)");

		if (m_RenderApi)
			return m_RenderApi;

		IRenderApi* api = m_Factory->CreateRenderAPI();
		if (!api)
		{
			CHE_CORE_ERROR("RenderResourceManager: failed to create render API");
			return nullptr;
		}

		api->Init(init_info);
		m_RenderApi = api;
		return m_RenderApi;
	}

	IRenderer* RenderResourceManager::InitRenderer(const RendererInitInfo& init_info)
	{
		CHE_CORE_ASSERT(m_Factory, "RenderResourceManager::InitRenderAPI — factory is null (Init not called)");

		if (m_Renderer)
			return m_Renderer;

		auto* api = InitRenderAPI(init_info);
		IRenderer* renderer = m_Factory->CreateRenderer(api);

		if (!renderer)
		{
			CHE_CORE_ERROR("RenderResourceManager: failed to create renderer");
			return nullptr;
		}
		m_Renderer = renderer;
		return m_Renderer;
	}

	IRenderApi* RenderResourceManager::GetRenderAPI() const
	{
		return m_RenderApi;
	}

	IRenderer* RenderResourceManager::GetRenderer() const
	{
		return m_Renderer;
	}

	TextureHandle RenderResourceManager::CreateTexture(const uint8_t* data, uint32_t width,
	                                                   uint32_t height, uint32_t channels)
	{
		ITexture* tex = m_Factory->CreateTexture(data, width, height, channels);
		if (!tex)
		{
			CHE_CORE_ERROR("RenderResourceManager: failed to create texture ({0}x{1})", width, height);
			return TextureHandle::Invalid();
		}
		return m_Textures.Add(tex);
	}

	TextureHandle RenderResourceManager::CreateTextureFromFile(const std::string& path)
	{
		int w = 0, h = 0, ch = 0;
		stbi_set_flip_vertically_on_load(1);
		uint8_t* pixels = stbi_load(path.c_str(), &w, &h, &ch, 0);
		if (!pixels || w <= 0 || h <= 0 || ch <= 0)
		{
			if (pixels) stbi_image_free(pixels);
			CHE_CORE_ERROR("RenderResourceManager: failed to load texture '{}'", path);
			return TextureHandle::Invalid();
		}

		TextureHandle handle = CreateTexture(pixels, static_cast<uint32_t>(w),
		                                     static_cast<uint32_t>(h), static_cast<uint32_t>(ch));
		stbi_image_free(pixels);

		if (handle.IsValid())
			CHE_CORE_INFO("RenderResourceManager: texture loaded '{}'", path);

		return handle;
	}

	FramebufferHandle RenderResourceManager::CreateFramebuffer(uint32_t width, uint32_t height)
	{
		IFramebuffer* fb = m_Factory->CreateFramebuffer(width, height);
		if (!fb)
		{
			CHE_CORE_ERROR("RenderResourceManager: failed to create framebuffer ({0}x{1})", width, height);
			return FramebufferHandle::Invalid();
		}
		return m_Framebuffers.Add(fb);
	}

	Ref<IVertexBuffer> RenderResourceManager::CreateVertexBuffer(float* vertices, uint32_t size)
	{
		IVertexBuffer* vb = m_Factory->CreateVertexBuffer(vertices, size);
		if (!vb)
		{
			CHE_CORE_ERROR("RenderResourceManager: failed to create vertex buffer");
			return nullptr;
		}
		return Ref<IVertexBuffer>(vb,
			[this](IVertexBuffer* ptr) { m_Factory->Delete(ptr); }
		);
	}

	Ref<IIndexBuffer> RenderResourceManager::CreateIndexBuffer(uint32_t* indices, uint32_t count)
	{
		IIndexBuffer* ib = m_Factory->CreateIndexBuffer(indices, count);
		if (!ib)
		{
			CHE_CORE_ERROR("RenderResourceManager: failed to create index buffer");
			return nullptr;
		}
		return Ref<IIndexBuffer>(ib,
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

	ITexture* RenderResourceManager::Get(TextureHandle h) const
	{
		return m_Textures.Get(h);
	}

	IFramebuffer* RenderResourceManager::Get(FramebufferHandle h) const
	{
		return m_Framebuffers.Get(h);
	}

	const Vector<ShaderEntry>& RenderResourceManager::GetShaderEntries() const 
	{
		return m_ShaderEntries; 
	}

	void RenderResourceManager::SetSceneCamera(const UBOCamera& camera)
	{
		m_SceneCamera = camera;
		m_SceneCameraValid = true;
	}

	void RenderResourceManager::InvalidateSceneCameraForFrame()
	{
		m_SceneCameraValid = false;
	}

	void RenderResourceManager::DestroyShader(ShaderHandle h)
	{
		m_Shaders.Remove(h);
	}

	void RenderResourceManager::DestroyVertexArray(VertexArrayHandle h)
	{
		m_VertexArrays.Remove(h);
	}

	void RenderResourceManager::DestroyRenderAPI()
	{
		if (!m_RenderApi)
			return;

		CHE_CORE_ASSERT(m_Factory, "RenderResourceManager::DestroyRenderAPI — factory is null");
		m_RenderApi->Shutdown();
		m_Factory->Delete(m_RenderApi);
		m_RenderApi = nullptr;
	}

	void RenderResourceManager::DestroyRenderer()
	{
		if (!m_Renderer)
			return;

		CHE_CORE_ASSERT(m_Factory, "RenderResourceManager::DestroyRenderAPI — factory is null");
		m_Factory->Delete(m_Renderer);
		m_Renderer = nullptr;
	}

	void RenderResourceManager::DestroyTexture(TextureHandle h)
	{
		m_Textures.Remove(h);
	}

	void RenderResourceManager::DestroyFramebuffer(FramebufferHandle h)
	{
		m_Framebuffers.Remove(h);
	}

	void RenderResourceManager::Shutdown()
	{
		m_ShaderWatcher.Clear();
		m_Shaders.Clear();
		m_VertexArrays.Clear();
		DestroyRenderer();
		DestroyRenderAPI();
		m_Textures.Clear();
		m_Framebuffers.Clear();
		m_ShaderEntries.clear();
		m_SceneCameraValid = false;
	}

}
