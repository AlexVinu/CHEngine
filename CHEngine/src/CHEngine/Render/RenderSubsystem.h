#pragma once

#include "Core.h"
#include "Render/IRenderFactory.h"
#include "Render/Graph/IRenderGraph.h"
#include "Render/IFrameGraphBackend.h"
#include "Render/UniformBlocks.h"
#include <CheStl/Vector.h>
#include "CHEngine/Utils/FileWatcher.h"

#include <functional>
#include <string>
#include <vector>
#include <memory>

namespace CHEngine
{
    class BasicFrameGraphFrontend;

    // Metadata for each named shader (hot-reload and Shader Manager panel).
    struct CHENGINE_API ShaderEntry
    {
        String       name;
        String       slangPath;
        String       vertEntry;
        String       fragEntry;
        ShaderHandle handle;
        bool         valid = false;
    };

    // Owns the render factory and all associated render-side state.
    // Lifetime: Application ctor → ~Application (in RAII order).
    class CHENGINE_API RenderSubsystem
    {
    public:
        // Initialises the subsystem immediately; factory must be non-null.
        RenderSubsystem(IRenderFactory* factory, const RendererInitInfo& init);
        ~RenderSubsystem();

        RenderSubsystem(const RenderSubsystem&) = delete;
        RenderSubsystem& operator=(const RenderSubsystem&) = delete;

        // ── Per-frame ─────────────────────────────────────────────────────────
        void BeginFrame();
        void EndFrame();

        // ── Frame graph ───────────────────────────────────────────────────────
        void          BeginFrameGraph();
        IRenderGraph& GetFrameGraph();
        void          EndFrameGraph();

        // ── Resource creation ─────────────────────────────────────────────────
        ShaderHandle  CreateShader(const String& slangSource,
                                   const String& vert = String("vertMain"),
                                   const String& frag = String("fragMain"),
                                   const String& sourcePath = String());

        ShaderHandle  CreateShaderFromFile(const String& name,
                                           const String& slangPath,
                                           const String& vert = String("vertMain"),
                                           const String& frag = String("fragMain"));

        TextureHandle CreateTexture(const uint8_t* data, uint32_t w, uint32_t h,
                                    uint32_t channels);
        TextureHandle CreateTextureFromFile(const std::string& path);

        // ── Resource destruction ──────────────────────────────────────────────
        void DestroyShader(ShaderHandle h);
        void DestroyTexture(TextureHandle h);

        // ── Factory access ────────────────────────────────────────────────────
        IRenderFactory* GetRenderFactory() const { return m_Factory; }

        // ── Viewport ─────────────────────────────────────────────────────────
        void          SetViewportSize(uint32_t w, uint32_t h);
        uint32_t      GetViewportWidth()  const { return m_ViewportW; }
        uint32_t      GetViewportHeight() const { return m_ViewportH; }
        void          SetViewport(uint32_t w, uint32_t h);

        void          SetViewportOutputTexture(TextureHandle h) { m_ViewportOutputTex = h; }
        TextureHandle GetViewportOutputTexture()          const { return m_ViewportOutputTex; }
        uint64_t      GetViewportColorTexID()             const;

        void          SetViewportHDRTexture(TextureHandle h)    { m_ViewportHDRTex = h; }
        TextureHandle GetViewportHDRTexture()             const { return m_ViewportHDRTex; }

        void          SetViewportDepthTexture(TextureHandle h)  { m_ViewportDepthTex = h; }
        TextureHandle GetViewportDepthTexture()           const { return m_ViewportDepthTex; }

        // Pre-scene hook: called by RenderSystem BEFORE MainColorPass.
        using PreSceneCallback = std::function<void()>;
        void              SetPreSceneCallback(PreSceneCallback cb);
        void              ClearPreSceneCallback();
        PreSceneCallback& GetPreSceneCallbackRef();

        // ── Default shaders ───────────────────────────────────────────────────
        void         SetDefaultMeshShader(ShaderHandle h)           { m_DefaultMeshShader = h; }
        ShaderHandle GetDefaultMeshShader()                  const  { return m_DefaultMeshShader; }

        void         SetDefaultSphereImpostorShader(ShaderHandle h) { m_DefaultSphereImpostorShader = h; }
        ShaderHandle GetDefaultSphereImpostorShader()        const  { return m_DefaultSphereImpostorShader; }

        // ── Per-frame scene camera ────────────────────────────────────────────
        // SetSceneCamera принимает уже скорректированную для текущего API VP
        // (clipCorr * logicalVP) — она идёт прямо в scene-шейдеры.
        // Параллельно вызывающий передаёт logical (GLM/OpenGL-convention) VP,
        // которая нужна системам, композирующим дополнительные трансформы поверх
        // (UI world-canvas), чтобы избежать двойного применения коррекции.
        void             SetSceneCamera(const UBOCamera& cam);
        void             SetSceneCameraLogicalVP(const glm::mat4& vp) { m_SceneCameraLogicalVP = vp; }
        const UBOCamera& GetSceneCamera()        const { return m_SceneCamera; }
        const glm::mat4& GetSceneCameraLogicalVP() const { return m_SceneCameraLogicalVP; }
        bool             HasSceneCamera()         const { return m_SceneCameraValid; }
        void             InvalidateSceneCamera()        { m_SceneCameraValid = false; }

        // ── Shader hot-reload ─────────────────────────────────────────────────
        void PollShaders();
        bool ReloadShader(ShaderHandle h);
        const std::vector<ShaderEntry>& GetShaderEntries() const { return m_ShaderEntries; }

    private:
        void Shutdown();   // called by dtor only
        bool ReloadShaderInternal(ShaderHandle h);

        IRenderFactory* m_Factory = nullptr;

        // ── Viewport ──────────────────────────────────────────────────────────
        uint32_t      m_ViewportW          = 1280u;
        uint32_t      m_ViewportH          = 720u;
        TextureHandle m_ViewportOutputTex;
        TextureHandle m_ViewportHDRTex;
        TextureHandle m_ViewportDepthTex;
        PreSceneCallback m_PreSceneCallback;

        // ── Frame graph ───────────────────────────────────────────────────────
        std::unique_ptr<BasicFrameGraphFrontend>       m_Graph;
        std::unique_ptr<IFrameGraphBackend>            m_GraphBackend;
        bool                                           m_GraphActive = false;

        // ── Default shaders ───────────────────────────────────────────────────
        ShaderHandle m_DefaultMeshShader;
        ShaderHandle m_DefaultSphereImpostorShader;

        // ── Scene camera ──────────────────────────────────────────────────────
        UBOCamera m_SceneCamera{};
        glm::mat4 m_SceneCameraLogicalVP{1.0f};
        bool      m_SceneCameraValid = false;

        // ── Shader hot-reload ─────────────────────────────────────────────────
        std::vector<ShaderEntry> m_ShaderEntries;
        FileWatcher              m_ShaderWatcher;
    };
}
