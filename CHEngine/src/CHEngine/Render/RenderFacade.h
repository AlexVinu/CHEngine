#pragma once

#include "Core.h"
#include "Render/IRenderFactory.h"
#include "Render/Graph/IRenderGraph.h"
#include "Render/IFrameGraphBackend.h"
#include "Render/UniformBlocks.h"
#include <CheStl/Vector.h>

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

    class CHENGINE_API RenderFacade
    {
    public:
        // ── Lifecycle ────────────────────────────────────────────────────────
        static void InitRenderer(const RendererInitInfo& init);
        static void Shutdown();

        // ── Per-frame ────────────────────────────────────────────────────────
        static void BeginFrame();
        static void EndFrame();

        // ── Frame graph (no-op stub until Phase 3) ───────────────────────────
        static void          BeginFrameGraph();
        static IRenderGraph& GetFrameGraph();
        static void          EndFrameGraph();

        // ── Resource creation ─────────────────────────────────────────────────
        static ShaderHandle  CreateShader(const String& slangSource,
                                          const String& vert = String("vertMain"),
                                          const String& frag = String("fragMain"),
                                          const String& sourcePath = String());

        static ShaderHandle  CreateShaderFromFile(const String& name,
                                                  const String& slangPath,
                                                  const String& vert = String("vertMain"),
                                                  const String& frag = String("fragMain"));

        static TextureHandle CreateTexture(const uint8_t* data, uint32_t w, uint32_t h,
                                           uint32_t channels);
        static TextureHandle CreateTextureFromFile(const std::string& path);

        // ── Resource destruction ──────────────────────────────────────────────
        static void DestroyShader(ShaderHandle h);
        static void DestroyTexture(TextureHandle h);

        // ── Low-level factory access ──────────────────────────────────────────
        static IRenderFactory* GetRenderFactory();

        // ── Viewport ─────────────────────────────────────────────────────────
        static void          SetViewportSize(uint32_t w, uint32_t h);
        static uint32_t      GetViewportWidth();
        static uint32_t      GetViewportHeight();

        // Final viewport color output as a TextureHandle, set by FG after Execute.
        // Sandbox/EditorViewport reads it for ImGui::Image.
        static void          SetViewportOutputTexture(TextureHandle h);
        static TextureHandle GetViewportOutputTexture();
        // Native texture pointer — for ImGui::Image. Returns 0 if unset.
        // MUST be uint64_t: on Metal/64-bit the value is a full pointer.
        static uint64_t      GetViewportColorTexID();

        // ── Default mesh shader ───────────────────────────────────────────────
        static void         SetDefaultMeshShader(ShaderHandle h);
        static ShaderHandle GetDefaultMeshShader();

        // ── Per-frame scene camera ────────────────────────────────────────────
        static void             SetSceneCamera(const UBOCamera& cam);
        static const UBOCamera& GetSceneCamera();
        static bool             HasSceneCamera();
        static void             InvalidateSceneCamera();

        // ── Legacy viewport viewport ──────────────────────────────────────────
        static void     SetViewport(uint32_t w, uint32_t h);

        // ── Shader hot-reload ─────────────────────────────────────────────────
        static void PollShaders();
        static bool ReloadShader(ShaderHandle h);
        static const std::vector<ShaderEntry>& GetShaderEntries();

    private:
        RenderFacade() = delete;
        ~RenderFacade() = delete;
        RenderFacade(const RenderFacade&) = delete;
        RenderFacade& operator=(const RenderFacade&) = delete;
        RenderFacade(RenderFacade&&) = delete;
        RenderFacade& operator=(RenderFacade&&) = delete;
    };
}
