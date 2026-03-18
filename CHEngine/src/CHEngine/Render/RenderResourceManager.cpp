#include "chepch.h"
#include "RenderResourceManager.h"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <stb_image.h>

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

		m_Textures = HandlePool<ITexture, TextureTag>(
			[this](ITexture* ptr) { m_Factory->Delete(ptr); }
		);

		m_Framebuffers = HandlePool<IFramebuffer, FramebufferTag>(
			[this](IFramebuffer* ptr) { m_Factory->Delete(ptr); }
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

	ShaderHandle RenderResourceManager::CreateShaderFromFile(const String& name,
	                                                         const String& vertexPath,
	                                                         const String& fragmentPath)
	{
		String vertexSrc, fragmentSrc;

		if (m_Factory && m_Factory->GetRenderApi() == ERenderAPI::METALL)
		{
			// Metal: derive .metal path from vertex shader path
			// e.g. "shaders/basic.vert" → "shaders/basic.metal"
			std::string path(vertexPath.c_str());
			auto dot = path.rfind('.');
			if (dot != std::string::npos)
				path = path.substr(0, dot) + ".metal";
			String metalSrc = ReadTextFile(String(path.c_str()));
			vertexSrc   = metalSrc;
			fragmentSrc = metalSrc; // ShaderMTL uses first arg only
		}
		else
		{
			vertexSrc   = ReadTextFile(vertexPath);
			fragmentSrc = ReadTextFile(fragmentPath);
		}

		bool valid = (vertexSrc.size() > 0 && fragmentSrc.size() > 0);

		ShaderHandle handle = ShaderHandle::Invalid();
		if (valid)
		{
			handle = CreateShader(vertexSrc, fragmentSrc);
			valid  = handle.IsValid();
		}

		if (valid)
			CHE_CORE_INFO("Loaded shader '{0}': '{1}' + '{2}'", name.c_str(), vertexPath.c_str(), fragmentPath.c_str());
		else
			CHE_CORE_ERROR("RenderResourceManager: failed to load shader '{0}'", name.c_str());

		ShaderEntry entry;
		entry.name     = name;
		entry.vertPath = vertexPath;
		entry.fragPath = fragmentPath;
		entry.handle   = handle;
		entry.valid    = valid;
		m_ShaderEntries.push_back(std::move(entry));

		// Автоматически регистрируем шейдер для hot reload
		if (valid) {
			auto reloadFn = [this, handle](const std::filesystem::path&) { ReloadShader(handle); };
			m_ShaderWatcher.Watch(std::filesystem::path(vertexPath.c_str()),   reloadFn);
			m_ShaderWatcher.Watch(std::filesystem::path(fragmentPath.c_str()), reloadFn);
		}

		return handle;
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

		String vertexSrc   = ReadTextFile(entry->vertPath);
		String fragmentSrc = ReadTextFile(entry->fragPath);

		if (vertexSrc.size() == 0 || fragmentSrc.size() == 0)
		{
			CHE_CORE_ERROR("ReloadShader: could not read files for '{0}'", entry->name.c_str());
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

		bool success = shader->Reload(vertexSrc, fragmentSrc);
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
			CHE_CORE_ERROR("RenderResourceManager: не удалось загрузить текстуру '{}'", path);
			return TextureHandle::Invalid();
		}

		TextureHandle handle = CreateTexture(pixels, static_cast<uint32_t>(w),
		                                     static_cast<uint32_t>(h), static_cast<uint32_t>(ch));
		stbi_image_free(pixels);

		if (handle.IsValid())
			CHE_CORE_INFO("RenderResourceManager: загружена текстура '{}'", path);

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

	ITexture* RenderResourceManager::Get(TextureHandle h) const
	{
		return m_Textures.Get(h);
	}

	IFramebuffer* RenderResourceManager::Get(FramebufferHandle h) const
	{
		return m_Framebuffers.Get(h);
	}

	const std::vector<ShaderEntry>& RenderResourceManager::GetShaderEntries() const 
	{ 
		return m_ShaderEntries; 
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
		m_RenderApis.Clear();
		m_Textures.Clear();
		m_Framebuffers.Clear();
		m_ShaderEntries.clear();
	}

}
