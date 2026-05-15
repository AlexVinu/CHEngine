#include "SceneViewLayer_ImGuiFrame.h"

#include "SceneViewLayer.h"
#include "SceneViewLayerAccess.h"
#include "UvEditorPanel.h"
#include "SceneViewLayerHost.h"
#include "SceneViewLayer_IO.h"
#include "SceneViewLayer_Render.h"
#include "SceneViewLayer_CameraOps.h"
#include "SceneViewLayer_MousePicking.h"
#include "SceneViewLayer_ShiftAMenu.h"
#include "SceneViewLayer_ShiftWMenu.h"
#include "SceneViewLayer_GizmoBar.h"

#include "UIThemeActive.h"
#include "TilingManager.h"

#include <imgui.h>
#include <Profiler.h>

// ── Helper: draw a panel in a tiling rect ─────────────────────────────────────
// Panels that receive (pos, size) keep their old signatures for now.
// Collapsed panels show only a title bar drawn by TilingManager — we skip Draw().

void RunSceneViewImGuiFrame(SceneViewLayer& layer)
{
    CHE_PROFILE_FUNCTION();
    SceneViewLayerHost host(layer);
    Sandbox::EditorViewport& viewport = SceneViewLayerAccess::Viewport(layer);
    Sandbox::TilingManager&  tiling   = SceneViewLayerAccess::Tiling(layer);

    viewport.Begin();
    SceneViewLayerCameraOps::PrepareEditorCameraFrame(layer);
    UIActive::SyncLayout();

    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    const float W = disp.x;
    const float H = disp.y;
    const auto& L = UIActive::g_Layout;

    EditorWorldContext* activeCtx = &SceneViewLayerAccess::Active(layer);
    const bool reset = activeCtx->ResetLayout;
    activeCtx->ResetLayout = false;
    if (reset)
        tiling.ResetLayout();

    // ── Toolbar (always fixed at top, not part of tiling) ─────────────────────
    const ImVec2 tbPos  = { 0.0f, 0.0f };
    const ImVec2 tbSize = { W, L.toolbarH };
    SceneViewLayerAccess::Toolbar(layer).Draw(host, tbPos, tbSize);
    activeCtx = &SceneViewLayerAccess::Active(layer);

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
        activeCtx = &SceneViewLayerAccess::Active(layer);
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

    // Profiler — in tiling, always draw (ignore m_ShowProfiler flag)
    drawIfVisible(PID::Profiler, [&]()
    {
        Sandbox::TileRect r = tiling.GetRect(PID::Profiler);
        if (r.valid)
        {
            ImGui::SetNextWindowPos(r.pos, ImGuiCond_Always);
            ImGui::SetNextWindowSize(r.size, ImGuiCond_Always);
        }
        SceneViewLayerAccess::Profiler(layer).Draw(host);
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

    // Scene Browser
    drawIfVisible(PID::SceneBrowser, [&]()
    {
        SceneViewLayerAccess::SceneBrowser(layer).OnImGuiRender(host);
    });

    // Script Editor — DrawInPanel() always shows window + hint when no file
    drawIfVisible(PID::ScriptEditor, [&]()
    {
        SceneViewLayerAccess::ScriptEditor(layer).DrawInPanel();
    });

    // ── Orbit indicator, tiling overlays ──────────────────────────────────────
    SceneViewLayerRender::DrawOrbitIndicator(layer);
    tiling.EndFrame();  // separators, close/collapse buttons, ghost
    SceneViewLayerShiftWMenu::Draw(layer);  // Shift+W panel picker popup

    viewport.End();
}
