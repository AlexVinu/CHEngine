#include "SceneViewLayer.h"

#include "SceneViewLayer_CameraOps.h"
#include "SceneViewLayer_IO.h"
#include "SceneViewLayer_Scripts.h"
#include "UvEditorPanel.h"
#include "SceneViewLayer_MousePicking.h"
#include "SceneViewLayer_ShiftAMenu.h"
#include "SceneViewLayer_ShiftWMenu.h"
#include "SceneViewLayer_GizmoBar.h"

#include "UIThemeActive.h"
#include "ProjectManager.h"

#include "TilingManager.h"
#include <CHEngine/Application.h>
#include <CHEngine/Scene/Components.h>

#include <imgui.h>

#include <CHEngine/Input/InputSystem.h>
#include <Log/Log.h>
#include <Profiler.h>
#include <filesystem>

SceneViewLayer::SceneViewLayer(Sandbox::EditorContext& ctx)
    : Layer("SceneView"), m_Ctx(ctx), m_ProjectManager(ctx.Projects)
{
    UIActive::SetTheme(AppTheme::RetroOS);
    UIActive::SyncLayout();

    if (m_ProjectManager->HasProject())
    {
        OnProjectOpened();
        SceneViewLayerIO::TryRestoreSession(m_Ctx);
    }
    else
    {
        // No project yet — create one empty session so panels have a valid context
        auto session = new EditorWorldContext(m_Ctx.Worlds.get());
        m_Ctx.Worlds->PushBack(session);
        m_Ctx.Gizmo.BindCommandStack(&session->CommandStack);
        SceneViewLayerCameraOps::ApplyOrbit(m_Ctx);
    }
}

void SceneViewLayer::OnProjectOpened()
{
    // Clear any sessions from a previous project and start fresh
    while (!m_Ctx.Worlds->IsEmpty())
        m_Ctx.Worlds->Erase(size_t(0));
    m_Ctx.ActiveIndex = 0;

    auto session = new EditorWorldContext(m_Ctx.Worlds.get());
    m_Ctx.Worlds->PushBack(session);
    session->UpdateState(std::nullopt, true);

    auto proj = m_ProjectManager->Current();
    m_ContentBrowser.SetAssetsDirectory(proj->AssetsAbsPath());

    // 1. Try startup scene
    const std::string& startupRel = proj->GetStartupScene();
    if (!startupRel.empty())
    {
        const std::string absPath = proj->ResolveScenePath(startupRel);
        if (std::filesystem::exists(absPath))
        {
            SceneViewLayerIO::LoadSceneSilent(m_Ctx, absPath);
			Finish();
            return;
        }
        CHE_CORE_WARN("SceneViewLayer: startup scene '{}' not found, scanning Scenes/", startupRel);
    }

    // 2. No startup scene (or missing file) — pick the first scene in Scenes/
    const std::filesystem::path scenesDir = proj->ScenesAbsPath();
    if (std::filesystem::is_directory(scenesDir))
    {
        for (const auto& entry : std::filesystem::directory_iterator(scenesDir))
        {
            if (entry.path().extension() == ".chscene")
            {
                SceneViewLayerIO::LoadSceneSilent(m_Ctx, entry.path().string());
				Finish();
                return;
            }
        }
    }

    // 3. No scenes found — empty session is already ready
	Finish();
}

void SceneViewLayer::Finish()
{
	SceneViewLayerCameraOps::ApplyOrbit(m_Ctx);
	m_Ctx.Gizmo.BindCommandStack(&m_Ctx.ActiveCtx()->CommandStack);
}

void SceneViewLayer::OnRenderUpdate(CHEngine::Timestep /*dt*/)
{
    CHE_PROFILE_FUNCTION();

    if (!m_Ctx.Worlds || m_Ctx.Worlds->IsEmpty())
        return;

    EditorWorldContext* active = m_Ctx.ActiveCtx();
    if (!active)
        return;

    m_Ctx.Gizmo.BindCommandStack(&active->CommandStack);

    // Thats fine if we draw only one scene per update
    // TODO: Do it in proper way
    m_Ctx.Viewport.BeginSceneRender(active);
}

void SceneViewLayer::OnEvent(CHEngine::Event& e)
{
    m_Ctx.Gizmo.OnEvent(e);
}

void SceneViewLayer::OnUIUpdate()
{
    if (!m_ProjectManager->HasProject())
    {
        if (m_ProjectBrowser.Draw(*m_ProjectManager))
            OnProjectOpened();
        return;
    }
    RunSceneViewImGuiFrame();
}

void SceneViewLayer::RunSceneViewImGuiFrame()
{
	CHE_PROFILE_FUNCTION();
	Sandbox::EditorViewport& viewport = m_Ctx.Viewport;
	Sandbox::TilingManager& tiling = m_Ctx.Tiling;

	viewport.Begin();
	SceneViewLayerCameraOps::PrepareEditorCameraFrame(m_Ctx);
	UIActive::SyncLayout();

	const ImVec2 disp = ImGui::GetIO().DisplaySize;
	const float W = disp.x;
	const float H = disp.y;
	const auto& L = UIActive::g_Layout;

	EditorWorldContext* activeCtx = m_Ctx.ActiveCtx();
	const bool reset = activeCtx->ResetLayout;
	activeCtx->ResetLayout = false;
	if (reset)
		tiling.ResetLayout();

	// ── Toolbar (always fixed at top, not part of tiling) ─────────────────────
	const ImVec2 tbPos = { 0.0f, 0.0f };
	const ImVec2 tbSize = { W, L.toolbarH };
	m_ToolbarPanel.Draw(m_Ctx, tbPos, tbSize);
	activeCtx = m_Ctx.ActiveCtx();

	// ── Tiling work area (below toolbar) ──────────────────────────────────────
	const ImVec2 workPos = { 0.0f, L.toolbarH };
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
			viewport.DrawImGui(m_Ctx.Gizmo,
				m_Ctx.CameraController,
				activeCtx,
				r.pos, r.size,
				activeCtx->GizmoOperation,
				activeCtx->GizmoMode);
			SceneViewLayerMousePicking::TryPick(m_Ctx);
			SceneViewLayerShiftAMenu::Draw(m_Ctx);
			SceneViewLayerGizmoBar::Draw(m_Ctx);
		});

	// Inspector (Camera + Scene tabs)
	drawIfVisible(PID::Inspector, [&]()
		{
			Sandbox::TileRect r = tiling.GetRect(PID::Inspector);
			if (r.valid)
				m_CameraPanel.Draw(m_Ctx, r.pos, r.size, reset);
			activeCtx = m_Ctx.ActiveCtx();
		});

	// Properties
	drawIfVisible(PID::Properties, [&]()
		{
			Sandbox::TileRect r = tiling.GetRect(PID::Properties);
			if (r.valid)
				m_PropertiesPanel.Draw(m_Ctx, r.pos, r.size, reset);
		});

	// Content Browser
	drawIfVisible(PID::ContentBrowser, [&]()
		{
			Sandbox::TileRect r = tiling.GetRect(PID::ContentBrowser);
			if (!r.valid) return;
			std::string action = m_ContentBrowser.OnImGuiRender(r.pos, r.size);
			if (!action.empty())
			{
				if (action.rfind("model:", 0) == 0)
					SceneViewLayerIO::ImportModel(m_Ctx, action.substr(6));
				else if (action.rfind("scene:", 0) == 0)
					SceneViewLayerIO::LoadScene(m_Ctx, action.substr(6));
				else if (action.rfind("script:", 0) == 0)
					m_Ctx.ScriptEditor.Open(action.substr(7));
				else if (action.rfind("shader:", 0) == 0)
					m_Ctx.ScriptEditor.OpenShader(action.substr(7));
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
			m_ProfilerPanel.DrawInPanel(m_Ctx);
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
			m_UvEditor.Draw(m_Ctx);
		});

	// Scene Browser — floating window, drawn outside tiling.
	// It is listed in Shift+W popup but intentionally rendered as a floating overlay,
	// NOT as a tiled panel. If placed into tiling via drag, the tile slot will be empty
	// (SceneBrowser draws itself floating regardless). This is by design — the scene
	// browser is a modal-style picker, not a dockable panel.
	if (!tiling.IsVisible(PID::SceneBrowser))
		m_Ctx.SceneBrowser.OnImGuiRender(m_Ctx);

	// Script Editor — DrawInPanel() always shows window + hint when no file
	drawIfVisible(PID::ScriptEditor, [&]()
		{
			Sandbox::TileRect r = tiling.GetRect(PID::ScriptEditor);
			if (r.valid)
			{
				ImGui::SetNextWindowPos(r.pos, ImGuiCond_Always);
				ImGui::SetNextWindowSize(r.size, ImGuiCond_Always);
			}

			auto& scriptEditor = m_Ctx.ScriptEditor;

			// Provide entity context when no script is currently open
			if (!scriptEditor.HasFile())
			{
				CHEngine::Scene* scene = activeCtx ? activeCtx->EditorScene.get() : nullptr;
				const CHEngine::EntityHandle selHandle = activeCtx ? activeCtx->SelectedEntity : CHEngine::EntityHandle{};
				CHEngine::Entity* selEnt = (scene && scene->IsEntityHandleValid(selHandle))
					? scene->TryGetEntity(selHandle) : nullptr;

				if (selEnt && selEnt->HasComponent<CHEngine::TagComponent>())
				{
					std::string name = scene->GetString(selEnt->GetComponent<CHEngine::TagComponent>().Name);
					bool hasScript = selEnt->HasComponent<CHEngine::ScriptComponent>();
					const auto* scriptComp = hasScript
						? &selEnt->GetComponent<CHEngine::ScriptComponent>() : nullptr;
					const auto* scriptVec = (scriptComp && scene)
						? scene->GetScripts(scriptComp->Scripts) : nullptr;
					std::string path = (scriptVec && !scriptVec->empty())
						? (*scriptVec)[0].Path : "";

					// Capture this (long-lived) instead of &host (local/frame-scoped)
					scriptEditor.SetEntityContext(name, hasScript, path,
						[this, selHandle, name]() {
							SceneViewLayerScripts::CreateAndAttachScript(m_Ctx, selHandle, name);
						},
						[this, path]() {
							SceneViewLayerScripts::OpenScriptInEditor(m_Ctx, path);
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
	SceneViewLayerRender::DrawOrbitIndicator(m_Ctx);

	// Tell tiling to skip drawing buttons inside the AI overlay area
	{
		auto& globalAi = m_GlobalAiOverlay;
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
	SceneViewLayerShiftWMenu::Draw(m_Ctx);  // Shift+W panel picker popup

	// ── Global AI Overlay (double-tap Z) ──────────────────────────────────────
	{
		auto& globalAi = m_GlobalAiOverlay;

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

		globalAi.Draw(m_Ctx);
	}

	// Export panel (floating dialog, shown when Export button clicked)
	m_Ctx.Export.Draw(m_Ctx);

	viewport.End();
}
