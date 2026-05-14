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

#include "UIThemeActive.h"

#include <imgui.h>
#include <Profiler.h>

void RunSceneViewImGuiFrame(SceneViewLayer& layer)
{
    CHE_PROFILE_FUNCTION();
    SceneViewLayerHost host(layer);
    Sandbox::EditorViewport& viewport = SceneViewLayerAccess::Viewport(layer);

    viewport.Begin();
    SceneViewLayerCameraOps::PrepareEditorCameraFrame(layer);
    UIActive::SyncLayout();

    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    const float W = disp.x;
    const float H = disp.y;

    const auto& L = UIActive::g_Layout;
    const float leftW = W * L.leftFrac;
    const float rightW = W * L.rightFrac;
    const float sH = H - L.toolbarH;

    EditorWorldContext* activeCtx = &SceneViewLayerAccess::Active(layer);
    const bool reset = activeCtx->ResetLayout;
    activeCtx->ResetLayout = false;

    const ImVec2 tbPos = { 0.0f, 0.0f };
    const ImVec2 tbSize = { W, L.toolbarH };
    const ImVec2 scenePos = { 0.0f, L.toolbarH };
    const ImVec2 sceneSize = { leftW, sH };
    const ImVec2 propsPos = { W - rightW, L.toolbarH };
    const ImVec2 propsSize = { rightW, sH * L.propsFrac };
    const ImVec2 camPos = { W - rightW, L.toolbarH + sH * L.propsFrac };
    const ImVec2 camSize = { rightW, sH * (1.0f - L.propsFrac) };

    SceneViewLayerAccess::Toolbar(layer).Draw(host, tbPos, tbSize);
    // Toolbar runs commands immediately (e.g. + Session → push_back may reallocate Sessions).
    activeCtx = &SceneViewLayerAccess::Active(layer);

    SceneViewLayerAccess::Hierarchy(layer).Draw(host, scenePos, sceneSize, reset);
    SceneViewLayerAccess::Properties(layer).Draw(host, propsPos, propsSize, reset);
    SceneViewLayerAccess::CameraPanel(layer).Draw(host, camPos, camSize, reset);
    activeCtx = &SceneViewLayerAccess::Active(layer);

    {
        const float vpH = sH - activeCtx->ContentBrowserHeight;
        const ImVec2 vpPos = { leftW, L.toolbarH };
        const ImVec2 vpSize = { W - leftW - rightW, vpH };
        viewport.DrawImGui(SceneViewLayerAccess::Gizmo(layer),
                           SceneViewLayerAccess::CameraController(layer),
                           activeCtx,
                           vpPos,
                           vpSize,
                           activeCtx->GizmoOperation,
                           activeCtx->GizmoMode);

        SceneViewLayerMousePicking::TryPick(layer);
        SceneViewLayerShiftAMenu::Draw(layer);
    }

    {
        const float bottomY = L.toolbarH + (sH - activeCtx->ContentBrowserHeight);
        const ImVec2 browserPos = { leftW, bottomY };
        const ImVec2 browserSize = { W - leftW - rightW, activeCtx->ContentBrowserHeight };
        std::string action = SceneViewLayerAccess::ContentBrowser(layer).OnImGuiRender(browserPos, browserSize);
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
    }

    SceneViewLayerAccess::ScriptEditor(layer).Draw();
    SceneViewLayerAccess::Profiler(layer).Draw(host);
    if (SceneViewLayerAccess::ShowUVEditor(layer))
        SceneViewLayerAccess::UvEditor(layer).Draw(host);
    SceneViewLayerAccess::SceneBrowser(layer).OnImGuiRender(host);
    SceneViewLayerRender::DrawOrbitIndicator(layer);
    viewport.End();
}
