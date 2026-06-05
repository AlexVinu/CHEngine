#include "chepch.h"

#include "RenderSubsystem.h"
#include "CHEngine/Application.h"
#include "CHEngine/Render/FrameGraph/BasicFrameGraphFrontend.h"
#include "CHEngine/Utils/AppPaths.h"
#include "FileSystem/FileSystem.h"

#include <stb_image.h>
#include <filesystem>

namespace CHEngine
{
    // ── Private helpers ───────────────────────────────────────────────────────

    static String ReadTextFile(const String& path)
    {
        const std::filesystem::path fsPath(path.c_str());
        if (!FileSystem::Exists(fsPath))
        {
            CHE_CORE_ERROR("RenderSubsystem: cannot open file '{}'", path.c_str());
            return {};
        }
        return String(FileSystem::ReadFileText(fsPath).c_str());
    }

    // ── Lifecycle ─────────────────────────────────────────────────────────────

    RenderSubsystem::RenderSubsystem(IRenderFactory* factory, const RendererInitInfo& init)
    {
        CHE_CORE_ASSERT(factory, "RenderSubsystem: factory must be non-null");
        m_Factory = factory;
        m_Factory->Init(init);
        CHE_CORE_INFO("RenderSubsystem initialised");
    }

    RenderSubsystem::~RenderSubsystem()
    {
        Shutdown();
    }

    void RenderSubsystem::Shutdown()
    {
        m_Graph.reset();
        m_GraphBackend.reset();
        m_ShaderWatcher.Clear();
        m_ShaderEntries.clear();
        m_SceneCameraValid   = false;
        m_ViewportOutputTex  = {};
        if (m_Factory)
        {
            m_Factory->Shutdown();
            m_Factory = nullptr;
        }
    }

    // ── Per-frame ─────────────────────────────────────────────────────────────

    bool RenderSubsystem::BeginFrame()
    {
        m_SceneCameraValid = false;
        if (m_Factory)
            return m_Factory->BeginFrame();
		CHE_CORE_WARN("RenderSubsystem::BeginFrame called without a valid factory");
        return false;
    }

    void RenderSubsystem::EndFrame()
    {
        if (m_Factory)
            m_Factory->EndFrame();
    }

    // ── Frame graph ───────────────────────────────────────────────────────────

    void RenderSubsystem::BeginFrameGraph()
    {
        if (!m_Graph)
            m_Graph = std::make_unique<BasicFrameGraphFrontend>();

        m_Graph->Reset();
        m_GraphActive = true;
    }

    IRenderGraph& RenderSubsystem::GetFrameGraph()
    {
        CHE_ASSERT(m_Graph && m_GraphActive,
                   "RenderSubsystem::GetFrameGraph called outside Begin/EndFrameGraph");
        return *m_Graph;
    }

    void RenderSubsystem::EndFrameGraph()
    {
        if (!m_Graph || !m_GraphActive)
        {
            CHE_CORE_WARN("RenderSubsystem::EndFrameGraph called without BeginFrameGraph");
            return;
        }

        m_Graph->Compile();

        if (!m_GraphBackend && m_Factory)
            m_GraphBackend = m_Factory->CreateFrameGraphBackend();

        if (m_GraphBackend)
            m_Graph->Execute(*m_GraphBackend);

        m_GraphActive = false;
    }

    // ── Resource creation ─────────────────────────────────────────────────────

    ShaderHandle RenderSubsystem::CreateShader(const String& slangSource,
                                               const String& vert,
                                               const String& frag,
                                               const String& sourcePath)
    {
        if (!m_Factory) return ShaderHandle::Invalid();
        return m_Factory->CreateShader(slangSource, vert, frag, sourcePath);
    }

    ShaderHandle RenderSubsystem::CreateShaderFromFile(const String& name,
                                                       const String& slangPath,
                                                       const String& vert,
                                                       const String& frag)
    {
        std::filesystem::path inputPath(slangPath.c_str());
        std::filesystem::path resolvedPath = inputPath.is_absolute()
            ? inputPath
            : (AppPaths::ExecutableDir() / inputPath);

        String src = ReadTextFile(String(resolvedPath.string().c_str()));
        const bool hasSource = !src.empty();

        String absPath;
        if (hasSource)
        {
            std::error_code ec;
            auto fs_abs = std::filesystem::absolute(resolvedPath, ec);
            absPath = ec ? String(resolvedPath.string().c_str())
                         : String(fs_abs.string().c_str());
        }

        ShaderHandle handle = ShaderHandle::Invalid();
        if (hasSource)
            handle = CreateShader(src, vert, frag, absPath);

        const bool valid = handle.IsValid();
        if (valid)
            CHE_CORE_INFO("Loaded shader '{}': '{}'", name.c_str(), slangPath.c_str());
        else
            CHE_CORE_ERROR("RenderSubsystem: failed to load shader '{}' from '{}'",
                           name.c_str(), slangPath.c_str());

        ShaderEntry entry;
        entry.name      = name;
        entry.slangPath = slangPath;
        entry.vertEntry = vert;
        entry.fragEntry = frag;
        entry.handle    = handle;
        entry.valid     = valid;
        m_ShaderEntries.push_back(std::move(entry));

        if (valid)
        {
            auto reload = [this, handle](const std::filesystem::path&) {
                ReloadShaderInternal(handle);
            };
            m_ShaderWatcher.Watch(resolvedPath, reload);
        }

        return handle;
    }

    TextureHandle RenderSubsystem::CreateTexture(const uint8_t* data,
                                                  uint32_t w, uint32_t h,
                                                  uint32_t channels)
    {
        if (!m_Factory) return TextureHandle::Invalid();
        return m_Factory->CreateTexture(data, w, h, channels,
                                        0, 1,
                                        TextureFormat::RGBA8_UNORM,
                                        TextureType::Texture2D,
                                        TextureUsage::ShaderResource,
                                        MemoryType::GpuOnly,
                                        String("texture"));
    }

    TextureHandle RenderSubsystem::CreateTextureFromFile(const std::string& path)
    {
        // Read raw bytes through the pak-aware FileSystem (pak first, disk fallback),
        // then decode from memory. This lets packed textures load in exported games;
        // in the editor (no pak mounted) it is a plain disk read.
        Buffer fileData = FileSystem::ReadFileBinary(std::filesystem::path(path));
        if (!fileData || fileData.Size == 0)
        {
            CHE_CORE_ERROR("RenderSubsystem: failed to read texture file '{}'", path.c_str());
            return TextureHandle::Invalid();
        }

        int w = 0, h = 0, channels = 0;
        stbi_set_flip_vertically_on_load(1);
        uint8_t* data = stbi_load_from_memory(fileData.Data,
                                              static_cast<int>(fileData.Size),
                                              &w, &h, &channels, 0);
        if (!data)
        {
            CHE_CORE_ERROR("RenderSubsystem: failed to decode texture '{}'", path.c_str());
            return TextureHandle::Invalid();
        }
        TextureHandle handle = CreateTexture(data, static_cast<uint32_t>(w),
                                             static_cast<uint32_t>(h),
                                             static_cast<uint32_t>(channels));
        stbi_image_free(data);
        return handle;
    }

    // ── Resource destruction ──────────────────────────────────────────────────

    void RenderSubsystem::DestroyShader(ShaderHandle h)
    {
        if (!h.IsValid()) return;
        auto it = std::find_if(m_ShaderEntries.begin(), m_ShaderEntries.end(),
                               [h](const ShaderEntry& e) { return e.handle == h; });
        if (it != m_ShaderEntries.end())
            m_ShaderEntries.erase(it);

        if (m_Factory) m_Factory->Delete(h);
    }

    void RenderSubsystem::DestroyTexture(TextureHandle h)
    {
        if (!h.IsValid()) return;
        if (m_Factory) m_Factory->Delete(h);
    }

    // ── Viewport ─────────────────────────────────────────────────────────────

    void RenderSubsystem::SetViewportSize(uint32_t w, uint32_t h)
    {
        if (w > 0) m_ViewportW = w;
        if (h > 0) m_ViewportH = h;
    }

    void RenderSubsystem::SetViewport(uint32_t w, uint32_t h)
    {
        SetViewportSize(w, h);
    }

    uint64_t RenderSubsystem::GetViewportColorTexID() const
    {
        if (!m_ViewportOutputTex.IsValid()) return 0;
        if (!m_Factory) return 0;
        return m_Factory->GetTextureNativeID(m_ViewportOutputTex);
    }

    void RenderSubsystem::SetPreSceneCallback(PreSceneCallback cb)  { m_PreSceneCallback = std::move(cb); }
    void RenderSubsystem::ClearPreSceneCallback()                   { m_PreSceneCallback = nullptr; }
    RenderSubsystem::PreSceneCallback& RenderSubsystem::GetPreSceneCallbackRef() { return m_PreSceneCallback; }

    // ── Scene camera ──────────────────────────────────────────────────────────

    void RenderSubsystem::SetSceneCamera(const UBOCamera& cam)
    {
        m_SceneCamera      = cam;
        m_SceneCameraValid = true;
    }

    // ── Shader hot-reload ─────────────────────────────────────────────────────

    void RenderSubsystem::PollShaders()
    {
        m_ShaderWatcher.Poll();
    }

    bool RenderSubsystem::ReloadShader(ShaderHandle h)
    {
        return ReloadShaderInternal(h);
    }

    bool RenderSubsystem::ReloadShaderInternal(ShaderHandle h)
    {
        ShaderEntry* entry = nullptr;
        for (ShaderEntry& e : m_ShaderEntries)
        {
            if (e.handle == h) { entry = &e; break; }
        }
        if (!entry) return false;

        String src = ReadTextFile(entry->slangPath);
        if (src.empty()) { entry->valid = false; return false; }

        if (!m_Factory) return false;

        String absPath;
        {
            std::error_code ec;
            auto fs_abs = std::filesystem::absolute(
                std::filesystem::path(entry->slangPath.c_str()), ec);
            absPath = ec ? entry->slangPath : String(fs_abs.string().c_str());
        }

        bool ok = m_Factory->ReloadShader(h, src, entry->vertEntry, entry->fragEntry, absPath);
        entry->valid = ok;

        if (ok)
            CHE_CORE_INFO("Reloaded shader '{}'", entry->name.c_str());
        else
            CHE_CORE_ERROR("Shader reload failed for '{}' — keeping old program", entry->name.c_str());

        return ok;
    }

} // namespace CHEngine
