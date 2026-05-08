#include "chepch.h"

#include "RenderFacade.h"
#include "CHEngine/Application.h"
#include "CHEngine/Render/FrameGraph/BasicFrameGraphFrontend.h"
#include "CHEngine/Utils/FileWatcher.h"
#include "FileSystem/FileSystem.h"

#include <stb_image.h>
#include <filesystem>

namespace CHEngine
{
    // ── Viewport ──────────────────────────────────────────────────────────────
    static uint32_t s_ViewportW = 1280u;
    static uint32_t s_ViewportH = 720u;

    // ── Viewport output texture (set by FG after Execute) ─────────────────────
    static TextureHandle s_ViewportOutputTex;

    // ── Frame graph ───────────────────────────────────────────────────────────
    static std::unique_ptr<BasicFrameGraphFrontend>  s_Graph;
    static std::unique_ptr<CHEngine::IFrameGraphBackend> s_GraphBackend;
    static bool s_GraphActive = false;

    // ── Default mesh shader ───────────────────────────────────────────────────
    static ShaderHandle s_DefaultMeshShader;

    // ── Per-frame scene camera ────────────────────────────────────────────────
    static UBOCamera s_SceneCamera{};
    static bool      s_SceneCameraValid = false;

    // ── Shader hot-reload data ────────────────────────────────────────────────
    static std::vector<ShaderEntry> s_ShaderEntries;
    static FileWatcher              s_ShaderWatcher;

    // ── Internal helpers ──────────────────────────────────────────────────────
    static IRenderFactory* GetFactory()
    {
        return Application::Get().GetRenderFactory();
    }

    static String ReadTextFile(const String& path)
    {
        const std::filesystem::path fsPath(path.c_str());
        if (!FileSystem::Exists(fsPath))
        {
            CHE_CORE_ERROR("RenderFacade: cannot open file '{}'", path.c_str());
            return {};
        }
        return String(FileSystem::ReadFileText(fsPath).c_str());
    }

    static bool ReloadShaderInternal(ShaderHandle h)
    {
        ShaderEntry* entry = nullptr;
        for (ShaderEntry& e : s_ShaderEntries)
        {
            if (e.handle == h) { entry = &e; break; }
        }
        if (!entry) return false;

        String src = ReadTextFile(entry->slangPath);
        if (src.empty()) { entry->valid = false; return false; }

        IRenderFactory* factory = GetFactory();
        if (!factory) return false;

        String absPath;
        {
            std::error_code ec;
            auto fs_abs = std::filesystem::absolute(std::filesystem::path(entry->slangPath.c_str()), ec);
            absPath = ec ? entry->slangPath : String(fs_abs.string().c_str());
        }

        bool ok = factory->ReloadShader(h, src, entry->vertEntry, entry->fragEntry, absPath);
        entry->valid = ok;

        if (ok)
            CHE_CORE_INFO("Reloaded shader '{}'", entry->name.c_str());
        else
            CHE_CORE_ERROR("Shader reload failed for '{}' — keeping old program", entry->name.c_str());

        return ok;
    }

} // namespace CHEngine

namespace CHEngine
{
    // ── Lifecycle ─────────────────────────────────────────────────────────────

    void RenderFacade::InitRenderer(const RendererInitInfo& init)
    {
        IRenderFactory* f = GetFactory();
        if (!f) {
            CHE_CORE_CRITICAL("RenderFacade::InitRenderer — no factory available");
            return;
        }
        f->Init(init);
        CHE_CORE_INFO("RenderFacade::InitRenderer succeeded");
    }

    void RenderFacade::Shutdown()
    {
        s_Graph.reset();
        s_GraphBackend.reset();
        s_ShaderWatcher.Clear();
        s_ShaderEntries.clear();
        s_SceneCameraValid = false;
        s_ViewportOutputTex = {};
        if (IRenderFactory* f = GetFactory())
            f->Shutdown();
    }

    // ── Per-frame ─────────────────────────────────────────────────────────────

    void RenderFacade::BeginFrame()
    {
        s_SceneCameraValid = false;
        if (IRenderFactory* f = GetFactory())
            f->BeginFrame();
    }

    void RenderFacade::EndFrame()
    {
        if (IRenderFactory* f = GetFactory())
            f->EndFrame();
    }

    // ── Frame graph ───────────────────────────────────────────────────────────

    void RenderFacade::BeginFrameGraph()
    {
        if (!s_Graph)
            s_Graph = std::make_unique<BasicFrameGraphFrontend>();

        s_Graph->Reset();
        s_GraphActive = true;
    }

    IRenderGraph& RenderFacade::GetFrameGraph()
    {
        CHE_ASSERT(s_Graph && s_GraphActive,
                   "RenderFacade::GetFrameGraph called outside BeginFrameGraph/EndFrameGraph");
        return *s_Graph;
    }

    void RenderFacade::EndFrameGraph()
    {
        if (!s_Graph || !s_GraphActive)
        {
            CHE_CORE_WARN("RenderFacade::EndFrameGraph called without matching BeginFrameGraph");
            return;
        }

        s_Graph->Compile();

        // Lazy-create backend on first use.
        if (!s_GraphBackend)
        {
            IRenderFactory* f = GetFactory();
            if (f)
                s_GraphBackend = f->CreateFrameGraphBackend();
        }

        if (s_GraphBackend)
            s_Graph->Execute(*s_GraphBackend);

        s_GraphActive = false;
    }

    // ── Resource creation ─────────────────────────────────────────────────────

    ShaderHandle RenderFacade::CreateShader(const String& slangSource,
                                            const String& vert,
                                            const String& frag,
                                            const String& sourcePath)
    {
        IRenderFactory* f = GetFactory();
        if (!f) return ShaderHandle::Invalid();
        return f->CreateShader(slangSource, vert, frag, sourcePath);
    }

    ShaderHandle RenderFacade::CreateShaderFromFile(const String& name,
                                                    const String& slangPath,
                                                    const String& vert,
                                                    const String& frag)
    {
        String src = ReadTextFile(slangPath);
        const bool hasSource = !src.empty();

        String absPath;
        if (hasSource)
        {
            std::error_code ec;
            auto fs_abs = std::filesystem::absolute(std::filesystem::path(slangPath.c_str()), ec);
            absPath = ec ? slangPath : String(fs_abs.string().c_str());
        }

        ShaderHandle handle = ShaderHandle::Invalid();
        if (hasSource)
            handle = CreateShader(src, vert, frag, absPath);

        const bool valid = handle.IsValid();
        if (valid)
            CHE_CORE_INFO("Loaded shader '{}': '{}'", name.c_str(), slangPath.c_str());
        else
            CHE_CORE_ERROR("RenderFacade: failed to load shader '{}' from '{}'",
                           name.c_str(), slangPath.c_str());

        ShaderEntry entry;
        entry.name      = name;
        entry.slangPath = slangPath;
        entry.vertEntry = vert;
        entry.fragEntry = frag;
        entry.handle    = handle;
        entry.valid     = valid;
        s_ShaderEntries.push_back(std::move(entry));

        if (valid)
        {
            auto reload = [handle](const std::filesystem::path&) { ReloadShaderInternal(handle); };
            s_ShaderWatcher.Watch(std::filesystem::path(slangPath.c_str()), reload);
        }

        return handle;
    }

    TextureHandle RenderFacade::CreateTexture(const uint8_t* data, uint32_t w, uint32_t h,
                                              uint32_t channels)
    {
        IRenderFactory* f = GetFactory();
        if (!f) return TextureHandle::Invalid();
        return f->CreateTexture(data, w, h, channels,
                                0, 1,
                                TextureFormat::RGBA8_UNORM,
                                TextureType::Texture2D,
                                TextureUsage::ShaderResource,
                                MemoryType::GpuOnly,
                                String("texture"));
    }

    TextureHandle RenderFacade::CreateTextureFromFile(const std::string& path)
    {
        int w = 0, h = 0, channels = 0;
        stbi_set_flip_vertically_on_load(1);
        uint8_t* data = stbi_load(path.c_str(), &w, &h, &channels, 0);
        if (!data)
        {
            CHE_CORE_ERROR("RenderFacade: failed to load texture '{}'", path.c_str());
            return TextureHandle::Invalid();
        }
        TextureHandle handle = CreateTexture(data, static_cast<uint32_t>(w),
                                             static_cast<uint32_t>(h),
                                             static_cast<uint32_t>(channels));
        stbi_image_free(data);
        return handle;
    }

    // ── Resource destruction ──────────────────────────────────────────────────

    void RenderFacade::DestroyShader(ShaderHandle h)
    {
        if (!h.IsValid()) return;
        // Remove from hot-reload list.
        auto it = std::find_if(s_ShaderEntries.begin(), s_ShaderEntries.end(),
                               [h](const ShaderEntry& e) { return e.handle == h; });
        if (it != s_ShaderEntries.end())
            s_ShaderEntries.erase(it);

        IRenderFactory* f = GetFactory();
        if (f) f->Delete(h);
    }

    void RenderFacade::DestroyTexture(TextureHandle h)
    {
        if (!h.IsValid()) return;
        IRenderFactory* f = GetFactory();
        if (f) f->Delete(h);
    }

    // ── Factory access ────────────────────────────────────────────────────────

    IRenderFactory* RenderFacade::GetRenderFactory()
    {
        return GetFactory();
    }

    // ── Viewport ─────────────────────────────────────────────────────────────

    void RenderFacade::SetViewportSize(uint32_t w, uint32_t h)
    {
        if (w > 0) s_ViewportW = w;
        if (h > 0) s_ViewportH = h;
    }

    uint32_t RenderFacade::GetViewportWidth()  { return s_ViewportW; }
    uint32_t RenderFacade::GetViewportHeight() { return s_ViewportH; }

    void RenderFacade::SetViewport(uint32_t w, uint32_t h)
    {
        SetViewportSize(w, h);
    }

    void RenderFacade::SetViewportOutputTexture(TextureHandle h)
    {
        s_ViewportOutputTex = h;
    }

    TextureHandle RenderFacade::GetViewportOutputTexture()
    {
        return s_ViewportOutputTex;
    }

    uint32_t RenderFacade::GetViewportColorTexID()
    {
        if (!s_ViewportOutputTex.IsValid()) return 0;
        IRenderFactory* f = GetFactory();
        if (!f) return 0;
        return static_cast<uint32_t>(f->GetTextureNativeID(s_ViewportOutputTex));
    }

    // ── Default mesh shader ───────────────────────────────────────────────────

    void RenderFacade::SetDefaultMeshShader(ShaderHandle h)
    {
        s_DefaultMeshShader = h;
    }

    ShaderHandle RenderFacade::GetDefaultMeshShader()
    {
        return s_DefaultMeshShader;
    }

    // ── Scene camera ──────────────────────────────────────────────────────────

    void RenderFacade::SetSceneCamera(const UBOCamera& cam)
    {
        s_SceneCamera      = cam;
        s_SceneCameraValid = true;
    }

    const UBOCamera& RenderFacade::GetSceneCamera()
    {
        return s_SceneCamera;
    }

    bool RenderFacade::HasSceneCamera()
    {
        return s_SceneCameraValid;
    }

    void RenderFacade::InvalidateSceneCamera()
    {
        s_SceneCameraValid = false;
    }

    // ── Shader hot-reload ─────────────────────────────────────────────────────

    void RenderFacade::PollShaders()
    {
        s_ShaderWatcher.Poll();
    }

    bool RenderFacade::ReloadShader(ShaderHandle h)
    {
        return ReloadShaderInternal(h);
    }

    const std::vector<ShaderEntry>& RenderFacade::GetShaderEntries()
    {
        return s_ShaderEntries;
    }

} // namespace CHEngine
