#include "SceneViewLayer.h"

#include "SceneViewLayerAccess.h"
#include "SceneViewLayer_CameraOps.h"
#include "SceneViewLayer_IO.h"

#include "UIThemeActive.h"
#include "ProjectManager.h"

#include <Log/Log.h>
#include <Profiler.h>
#include <filesystem>

namespace
{
	void DrawOrbitIndicator(SceneViewLayer& layer)
	{
		Ref<EditorWorldContext> ctx = SceneViewLayerAccess::ActiveRef(layer);
		if (ctx->GetSessionState() == SceneSession::State::Play || ctx->GetSessionState() == SceneSession::State::Pause)
			return;

		ImGuiIO& io = ImGui::GetIO();
		const float W = io.DisplaySize.x;
		const float H = io.DisplaySize.y;

		auto* viewport_camera = ctx->ViewportCamera.get();
		if (!viewport_camera)
			return;
		glm::mat4 view = viewport_camera->GetViewMatrix();
		glm::mat4 proj = viewport_camera->GetProjectionMatrix();

		glm::vec4 clip = proj * view * glm::vec4(ctx->EditorCameraState.OrbitTarget, 1.0f);
		if (clip.w <= 0.0f)
			return;

		glm::vec3 ndc = glm::vec3(clip) / clip.w;
		if (glm::abs(ndc.x) > 1.0f || glm::abs(ndc.y) > 1.0f)
			return;

		float sx = (ndc.x + 1.0f) * 0.5f * W;
		float sy = (1.0f - ndc.y) * 0.5f * H;

		ImDrawList* dl = ImGui::GetBackgroundDrawList();
		const float r = 6.0f;
		const float gap = 2.5f;
		const ImU32 col = IM_COL32(255, 255, 255, 140);
		const ImU32 out = IM_COL32(0, 0, 0, 80);

		dl->AddLine({ sx - r - 1, sy }, { sx - gap - 1, sy }, out, 1.5f);
		dl->AddLine({ sx + gap - 1, sy }, { sx + r - 1, sy }, out, 1.5f);
		dl->AddLine({ sx, sy - r - 1 }, { sx, sy - gap - 1 }, out, 1.5f);
		dl->AddLine({ sx, sy + gap - 1 }, { sx, sy + r - 1 }, out, 1.5f);

		dl->AddLine({ sx - r, sy }, { sx - gap, sy }, col, 1.5f);
		dl->AddLine({ sx + gap, sy }, { sx + r, sy }, col, 1.5f);
		dl->AddLine({ sx, sy - r }, { sx, sy - gap }, col, 1.5f);
		dl->AddLine({ sx, sy + gap }, { sx, sy + r }, col, 1.5f);

		dl->AddCircleFilled({ sx, sy }, 1.8f, col);
	}
}

SceneViewLayer::SceneViewLayer(Ref<std::vector<Ref<EditorWorldContext>>> worlds,
                                    Ref<ProjectManager> proj_manager)
    : Layer("SceneView"), m_EditorWorldContexts(worlds), m_ProjectManager(proj_manager), m_ActiveIndex(0)
{
    auto ctx = MakeRef<EditorWorldContext>();
    m_EditorWorldContexts->emplace_back(ctx);

    m_GizmoSystem.BindCommandStack(&SceneViewLayerAccess::ActiveRef(*this)->CommandStack);

    UIActive::SetTheme(AppTheme::RetroOS);
    UIActive::SyncLayout();

    SceneViewLayerCameraOps::ApplyOrbit(*this);

    if (m_ProjectManager->HasProject())
    {
        // Project was already loaded by SandboxApp — init editor state.
        OnProjectOpened();
        SceneViewLayerIO::TryRestoreSession(*this);
    }
}

void SceneViewLayer::OnProjectOpened()
{
    Project* proj = m_ProjectManager->Current();
    if (!proj)
        return;

    // Reset to a single clean session when switching projects.
    m_EditorWorldContexts->resize(1);

    (*m_EditorWorldContexts)[0] = MakeRef<EditorWorldContext>();

    SceneViewLayerAccess::SetActiveIndex(*this, 0);

    SceneViewLayerCameraOps::ApplyOrbit(*this);
    m_GizmoSystem.BindCommandStack(&SceneViewLayerAccess::ActiveRef(*this)->CommandStack);

    m_ContentBrowser.SetAssetsDirectory(proj->AssetsAbsPath());

    // Load startup scene.
    const std::string& startupRel = proj->GetStartupScene();
    if (!startupRel.empty())
    {
        const std::string absPath = proj->ResolveScenePath(startupRel);
        if (std::filesystem::exists(absPath))
            SceneViewLayerIO::LoadSceneSilent(*this, absPath);
    }
}

void SceneViewLayer::OnUpdate(CHEngine::Timestep dt)
{
    CHE_PROFILE_FUNCTION();
    if (!m_ProjectManager->HasProject())
        return;

    if (m_EditorWorldContexts->empty())
        return;

    Ref<EditorWorldContext> active = SceneViewLayerAccess::ActiveRef(*this);
    m_GizmoSystem.BindCommandStack(&active->CommandStack);
    
    // Thats fine if we draw only one scene per update
    // TODO: Do it in proper way
    m_Viewport.BeginSceneRender(active);
}

void SceneViewLayer::OnEvent(CHEngine::Event& e)
{
    m_CameraController.OnEvent(e);
    m_GizmoSystem.OnEvent(e);
}

void SceneViewLayer::OnImGuiRender()
{
    if (!m_ProjectManager->HasProject())
    {
        if (m_ProjectBrowser.Draw())
            OnProjectOpened();
        return;
    }
    RunSceneViewImGuiFrame();
}

void SceneViewLayer::RunSceneViewImGuiFrame()
{
	CHE_PROFILE_FUNCTION();
	SceneViewLayerHost host(*this);
	Sandbox::EditorViewport& viewport = SceneViewLayerAccess::Viewport(*this);

	viewport.Begin();
	SceneViewLayerCameraOps::PrepareEditorCameraFrame(*this);
	UIActive::SyncLayout();

	const ImVec2 disp = ImGui::GetIO().DisplaySize;
	const float W = disp.x;
	const float H = disp.y;

	const auto& L = UIActive::g_Layout;
	const float leftW = W * L.leftFrac;
	const float rightW = W * L.rightFrac;
	const float sH = H - L.toolbarH;

	Ref<EditorWorldContext> activeCtx = SceneViewLayerAccess::ActiveRef(*this);
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

	SceneViewLayerAccess::Toolbar(*this).Draw(host, tbPos, tbSize);
	// Toolbar runs commands immediately (e.g. + Session → push_back may reallocate Sessions).
	activeCtx = SceneViewLayerAccess::ActiveRef(layer);

	SceneViewLayerAccess::Hierarchy(layer).Draw(host, scenePos, sceneSize, reset);
	SceneViewLayerAccess::Properties(layer).Draw(host, propsPos, propsSize, reset);
	SceneViewLayerAccess::CameraPanel(layer).Draw(host, camPos, camSize, reset);
	activeCtx = SceneViewLayerAccess::ActiveRef(layer);

	{
		const float vpH = sH - activeCtx->ContentBrowserHeight;
		const ImVec2 vpPos = { leftW, L.toolbarH };
		const ImVec2 vpSize = { W - leftW - rightW, vpH };
		viewport.DrawImGui(SceneViewLayerAccess::Gizmo(layer),
			SceneViewLayerAccess::CameraController(layer),
			activeCtx.get(),
			vpPos,
			vpSize,
			activeCtx->GizmoOperation,
			activeCtx->GizmoMode);
	}

	{
		const float bottomY = L.toolbarH + (sH - activeCtx->ContentBrowserHeight);
		const ImVec2 browserPos = { leftW, bottomY };
		const ImVec2 browserSize = { W - leftW - rightW, activeCtx->ContentBrowserHeight };
		std::string action = SceneViewLayerAccess::ContentBrowser(*this).OnImGuiRender(browserPos, browserSize);
		if (!action.empty())
		{
			if (action.rfind("model:", 0) == 0)
				host.ImportModel(action.substr(6));
			else if (action.rfind("scene:", 0) == 0)
				SceneViewLayerIO::LoadScene(*this, action.substr(6));
			else if (action.rfind("script:", 0) == 0)
				SceneViewLayerAccess::ScriptEditor(*this).Open(action.substr(7));
			else if (action.rfind("shader:", 0) == 0)
				SceneViewLayerAccess::ScriptEditor(*this).OpenShader(action.substr(7));
		}
	}

	SceneViewLayerAccess::ScriptEditor(*this).Draw();
	SceneViewLayerAccess::Profiler(*this).Draw(host);
	SceneViewLayerAccess::SceneBrowser(*this).OnImGuiRender(host);
	DrawOrbitIndicator(*this);
	viewport.End();
}
