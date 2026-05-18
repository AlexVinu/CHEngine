#include "SceneViewLayer.h"
#include "SceneViewLayerAccess.h"
#include "UvEditorPanel.h"
#include "SceneViewLayerHost.h"
#include "SceneViewLayer_IO.h"
#include "SceneViewLayer_CameraOps.h"
#include "SceneViewLayer_MousePicking.h"
#include "SceneViewLayer_ShiftAMenu.h"
#include "SceneViewLayer_ShiftWMenu.h"
#include "SceneViewLayer_GizmoBar.h"

#include "UIThemeActive.h"
#include "TilingManager.h"
#include <CHEngine/Input/InputSystem.h>

#include <CHEngine/Scene/Components.h>

#include <imgui.h>
#include <Profiler.h>

// ── Helper: draw a panel in a tiling rect ─────────────────────────────────────
// Panels that receive (pos, size) keep their old signatures for now.
// Collapsed panels show only a title bar drawn by TilingManager — we skip Draw().

void RunSceneViewImGuiFrame(SceneViewLayer& layer)
{
    CHE_PROFILE_FUNCTION();
    Sandbox::SceneViewLayerHost host(layer);
    Sandbox::EditorViewport& viewport = SceneViewLayerAccess::Viewport(layer);
    Sandbox::TilingManager&  tiling   = SceneViewLayerAccess::Tiling(layer);

    CHEngine::GetInputSystem().BeginFrame();
    viewport.Begin();
    SceneViewLayerCameraOps::PrepareEditorCameraFrame(layer);
    UIActive::SyncLayout();

    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    const float W = disp.x;
    const float H = disp.y;
    const auto& L = UIActive::g_Layout;

    Ref<EditorWorldContext> activeRef = SceneViewLayerAccess::ActiveRef(layer);
    EditorWorldContext* activeCtx = activeRef.get();
    const bool reset = activeCtx->ResetLayout;
    activeCtx->ResetLayout = false;
    if (reset)
        tiling.ResetLayout();

    // ── Toolbar (always fixed at top, not part of tiling) ─────────────────────
    const ImVec2 tbPos  = { 0.0f, 0.0f };
    const ImVec2 tbSize = { W, L.toolbarH };
    SceneViewLayerAccess::Toolbar(layer).Draw(host, tbPos, tbSize);
    activeRef = SceneViewLayerAccess::ActiveRef(layer);
    activeCtx = activeRef.get();

    // ── Tiling work area (below toolbar) ──────────────────────────────────────
    const ImVec2 workPos  = { 0.0f, L.toolbarH };
    const ImVec2 workSize = { W, H - L.toolbarH };
    tiling.BeginFrame(workPos, workSize);

    // ── Draw each tiled panel ─────────────────────────────────────────────────
    using PID = Sandbox::PanelID;

    // Helper lambda: draw panel if visible and not collapsed
    auto drawIfVisible = [&](PID id, auto drawFn)
    {
        if (!tiling.IsVisible(id)) return;
        if (tiling.IsCollapsed(id))
        {
            // Collapsed: show only the styled title bar (same look as full panel)
            Sandbox::TileRect r = tiling.GetRect(id);
            if (r.valid)
                UIActive::BeginPanel(Sandbox::PanelTitle(id), r.pos, r.size,
                                     ImGuiWindowFlags_NoScrollbar |
                                     ImGuiWindowFlags_NoBringToFrontOnFocus,
                                     false);
            UIActive::EndPanel();
            return;
        }
        drawFn();
    };

    // Viewport
    drawIfVisible(PID::Viewport, [&]()
    {
        Sandbox::TileRect r = tiling.GetRect(PID::Viewport);
        if (!r.valid) return;
        viewport.DrawImGui(SceneViewLayerAccess::Gizmo(layer),
                           SceneViewLayerAccess::CameraController(layer),
                           activeCtx,
                           r.pos, r.size,
                           activeCtx->GizmoOperation,
                           activeCtx->GizmoMode);
        SceneViewLayerMousePicking::TryPick(layer);
        SceneViewLayerShiftAMenu::Draw(layer);
        SceneViewLayerGizmoBar::Draw(layer);
    });

    // Inspector (Camera + Scene tabs)
    drawIfVisible(PID::Inspector, [&]()
    {
        Sandbox::TileRect r = tiling.GetRect(PID::Inspector);
        if (r.valid)
            SceneViewLayerAccess::CameraPanel(layer).Draw(host, r.pos, r.size, reset);
        activeRef = SceneViewLayerAccess::ActiveRef(layer);
        activeCtx = activeRef.get();
    });

    // Properties
    drawIfVisible(PID::Properties, [&]()
    {
        Sandbox::TileRect r = tiling.GetRect(PID::Properties);
        if (r.valid)
            SceneViewLayerAccess::Properties(layer).Draw(host, r.pos, r.size, reset);
    });

    // Content Browser
    drawIfVisible(PID::ContentBrowser, [&]()
    {
        Sandbox::TileRect r = tiling.GetRect(PID::ContentBrowser);
        if (!r.valid) return;
        std::string action = SceneViewLayerAccess::ContentBrowser(layer)
                                .OnImGuiRender(r.pos, r.size);
        if (!action.empty())
        {
            if (action.rfind("model:", 0) == 0)
                host.ImportModel(action.substr(6));
            else if (action.rfind("scene:", 0) == 0)
                SceneViewLayerIO::LoadScene(layer, action.substr(6));
            else if (action.rfind("script:", 0) == 0)
                SceneViewLayerAccess::ScriptEditor(layer).Open(action.substr(7));
            else if (action.rfind("shader:", 0) == 0)
                SceneViewLayerAccess::ScriptEditor(layer).OpenShader(action.substr(7));
        }
    });

    // Profiler — in tiling, always draw regardless of m_ShowProfiler flag
    drawIfVisible(PID::Profiler, [&]()
    {
        Sandbox::TileRect r = tiling.GetRect(PID::Profiler);
        if (r.valid)
        {
            ImGui::SetNextWindowPos(r.pos, ImGuiCond_Always);
            ImGui::SetNextWindowSize(r.size, ImGuiCond_Always);
        }
        SceneViewLayerAccess::Profiler(layer).DrawInPanel(host);
    });

    // UV Editor — in tiling, always draw (ignore m_ShowUVEditor flag)
    drawIfVisible(PID::UVEditor, [&]()
    {
        Sandbox::TileRect r = tiling.GetRect(PID::UVEditor);
        if (r.valid)
        {
            ImGui::SetNextWindowPos(r.pos, ImGuiCond_Always);
            ImGui::SetNextWindowSize(r.size, ImGuiCond_Always);
        }
        SceneViewLayerAccess::UvEditor(layer).Draw(host);
    });

    // Scene Browser — floating window, drawn outside tiling.
    // It is listed in Shift+W popup but intentionally rendered as a floating overlay,
    // NOT as a tiled panel. If placed into tiling via drag, the tile slot will be empty
    // (SceneBrowser draws itself floating regardless). This is by design — the scene
    // browser is a modal-style picker, not a dockable panel.
    if (!tiling.IsVisible(PID::SceneBrowser))
        SceneViewLayerAccess::SceneBrowser(layer).OnImGuiRender(host);

    // Script Editor — DrawInPanel() always shows window + hint when no file
    drawIfVisible(PID::ScriptEditor, [&]()
    {
        Sandbox::TileRect r = tiling.GetRect(PID::ScriptEditor);
        if (r.valid)
        {
            ImGui::SetNextWindowPos(r.pos, ImGuiCond_Always);
            ImGui::SetNextWindowSize(r.size, ImGuiCond_Always);
        }

        auto& scriptEditor = SceneViewLayerAccess::ScriptEditor(layer);

        // Provide entity context when no script is currently open
        if (!scriptEditor.HasFile())
        {
            CHEngine::Scene* scene = activeCtx ? activeCtx->EditorScene.get() : nullptr;
            const CHEngine::EntityHandle selHandle = activeCtx ? activeCtx->SelectedEntity : CHEngine::EntityHandle{};
            CHEngine::Entity* selEnt = (scene && scene->IsEntityHandleValid(selHandle))
                                       ? scene->TryGetEntity(selHandle) : nullptr;

            if (selEnt && selEnt->HasComponent<CHEngine::TagComponent>())
            {
                std::string name = selEnt->GetComponent<CHEngine::TagComponent>().Name;
                bool hasScript   = selEnt->HasComponent<CHEngine::ScriptComponent>();
                const auto* scriptComp = hasScript
                    ? &selEnt->GetComponent<CHEngine::ScriptComponent>() : nullptr;
                std::string path = (scriptComp && !scriptComp->Scripts.empty())
                    ? scriptComp->Scripts[0].Path : "";

                // Capture &layer (long-lived) instead of &host (local/frame-scoped)
                scriptEditor.SetEntityContext(name, hasScript, path,
                    [&layer, selHandle, name]() {
                        Sandbox::SceneViewLayerHost h(layer);
                        h.CreateAndAttachScript(selHandle, name);
                    },
                    [&layer, path]() {
                        Sandbox::SceneViewLayerHost h(layer);
                        h.OpenScriptInEditor(path);
                    });
            }
            else
            {
                scriptEditor.ClearEntityContext();
            }
        }
        else
        {
            scriptEditor.ClearEntityContext();
        }

        scriptEditor.DrawInPanel();
    });

    // ── Orbit indicator, tiling overlays ──────────────────────────────────────
    SceneViewLayerRender::DrawOrbitIndicator(layer);

    // Tell tiling to skip drawing buttons inside the AI overlay area
    {
        auto& globalAi = SceneViewLayerAccess::GlobalAi(layer);
        if (globalAi.IsVisible())
        {
            const float oW = std::min(W * 0.60f, 800.0f);
            const float oH = 340.0f; // max height (settings open)
            tiling.SetOverlayBlock(
                ImVec2((W - oW) * 0.5f, H - oH - 48.0f),
                ImVec2(oW, oH), true);
        }
        else
        {
            tiling.SetOverlayBlock({}, {}, false);
        }
    }

    tiling.EndFrame();  // separators, close/collapse buttons, ghost
    SceneViewLayerShiftWMenu::Draw(layer);  // Shift+W panel picker popup

    // ── Global AI Overlay (double-tap Z) ──────────────────────────────────────
    {
        auto& globalAi = SceneViewLayerAccess::GlobalAi(layer);

        // Detect double-tap Z — ignore when typing in any text input
        if (!ImGui::GetIO().WantTextInput)
        {
            static double s_LastZTime = -1.0; // double avoids float overflow in long sessions
            if (ImGui::IsKeyPressed(ImGuiKey_Z, false))
            {
                double now = ImGui::GetTime();
                if (now - s_LastZTime < 0.35)
                {
                    globalAi.Toggle();
                    s_LastZTime = -1.0;
                }
                else
                {
                    s_LastZTime = now;
                }
            }
        }

        globalAi.Draw(host);
    }

    // Export panel (floating dialog, shown when Export button clicked)
    SceneViewLayerAccess::Export(layer).Draw(host);

    viewport.End();
}
