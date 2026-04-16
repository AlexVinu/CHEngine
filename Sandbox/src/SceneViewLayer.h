#pragma once

#include <CHEngine.h>
#include <CHEngine/SceneSession.h>
#include <CHEngine/World/World.h>
#include <ImGuizmo.h>
#include <nlohmann/json.hpp>
#include "UIThemeActive.h"
#include "UndoStack.h"
#include "ContentBrowserPanel.h"

#include <glm/glm.hpp>
#include <boost/uuid/nil_generator.hpp>

#include <string>

using EditorState = SceneSession::State;

// ============================================================================
//  SceneViewLayer — main 3D editor layer for Sandbox
//
//  Panels
//    Toolbar     — gizmo mode, grid, snap, hotkeys (T/R/S)
//    Scene       — object hierarchy, import button
//    Properties  — transform, material, mesh info
//    Camera      — view presets, orbit controls
//
//  Camera controls (Blender-style, macOS trackpad)
//    RMB drag / two-finger drag → orbit (вращение вокруг цели)
//    Alt+LMB drag               → orbit
//    MMB drag                   → orbit
//    Alt+Shift+LMB drag         → pan (рука)
//    Shift+MMB drag             → pan
//    Scroll up/down             → zoom
//    Ctrl+Scroll                → orbit pitch
//    F                          → frame selected
//    Shift + gizmo drag         → snap (1 unit / 45° / 0.1)
// ============================================================================
class SceneViewLayer : public CHEngine::Layer
{
public:
    SceneViewLayer();

    void OnUpdate(CHEngine::Timestep dt) override;
    void OnImGuiRender() override;
    void OnEvent(CHEngine::Event& e) override;

private:
    SceneSession& GetOrCreateActiveSession();
    SceneSession& GetActiveSceneSession();
    const SceneSession& GetActiveSceneSession() const;
    void SetActiveSceneSessionIndex(size_t session_index);

    void UpdateSceneSession(SceneSession& scene_session, CHEngine::Timestep dt);
    // ── Orbit camera ─────────────────────────────────────────────────────────
    void ApplyOrbit();
    void SetViewPreset(float yaw, float pitch);
    void FocusOnSelected();
    void UpdateCameraInput();

    // ── Grid / axes geometry ─────────────────────────────────────────────────
    void BuildGrid();

    // ── Rendering ────────────────────────────────────────────────────────────
    void RenderScene(CHEngine::Timestep dt);

    // ── UI panels ────────────────────────────────────────────────────────────
    void DrawToolbar      (ImVec2 pos, ImVec2 size);
    void DrawScenePanel   (ImVec2 pos, ImVec2 size, bool resetSize);
    void DrawPropsPanel   (ImVec2 pos, ImVec2 size, bool resetSize);
    void DrawCameraPanel  (ImVec2 pos, ImVec2 size, bool resetSize);
    void DrawProfilerPanel();
    void DrawOrbitIndicator();
    void DrawGizmo();

    // ── Model import ─────────────────────────────────────────────────────────
    void ImportModel(const std::string& filepath);

    // ── Scene serialization ───────────────────────────────────────────────────
    void SaveScene();
    void LoadScene(const std::string& path = "");

    // ── Play / Pause / Stop ───────────────────────────────────────────────────
    void EnterPlayMode();
    void EnterPauseMode();
    void ResumeFromPause();
    void StopPlayMode();
    void StepOneFrame();

    // Сохраняет/восстанавливает сцену при переключении render API
    void AutoSaveForRestart();
    void TryRestoreSession();

    // Runs one Simulation-phase tick so queued physics events are consumed (dt may be zero).

    static constexpr const char* k_SessionFile      = ".che_session.chscene";
    static constexpr const char* k_SessionStateFile = ".che_session_state";

    // =========================================================================
    // State
    // =========================================================================

    // Scenes/SceneSessions
    std::vector<SceneSession> m_SceneSessions;
    size_t m_ActiveSessionIndex = 0;

    // Cached viewport aspect (used by camera / rendering math)
    float m_AspectRatio = 1280.0f / 720.0f;

    // Editor-camera entity (hidden in hierarchy while editing)
    CHEngine::EntityHandle m_EditorCameraEntity{};

    // Play-mode snapshot (editor scene → runtime scene)
    nlohmann::json m_SceneSnapshot{};
    bool m_HasSnapshot = false;

    CHEngine::ShaderHandle      m_MeshShader;
    CHEngine::ShaderHandle      m_GridShader;
    CHEngine::VertexArrayHandle m_GridVAO;

    // Framebuffer (offscreen render target)
    CHEngine::FramebufferHandle m_Framebuffer;
    ImVec2                      m_ViewportPos     = { 0.0f,   0.0f };
    ImVec2                      m_ViewportSize    = { 1280.0f, 720.0f };
    bool                        m_ViewportHovered = false;

    // Gizmo
    ImGuizmo::OPERATION m_GizmoOperation = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE      m_GizmoMode      = ImGuizmo::WORLD;
    bool                m_LocalMode      = false;

    // Viewport flags
    bool m_ShowGrid      = true;
    bool m_ShowProfiler  = false;
    bool m_ResetLayout   = false;   // true → snap panel sizes back next frame

    // Undo
    UndoStack               m_UndoStack;
    CHEngine::Transform     m_TransformBeforeDrag;
    bool                    m_GizmoWasUsing = false;

    // Recent files
    CHEngine::RecentFiles   m_RecentFiles;

    // Camera vars
    glm::vec3 m_OrbitTarget = { 0.0f, 0.0f, 0.0f };
    float     m_OrbitDist = 8.0f;
    bool      m_FollowObject = false;

    // Content Browser (bottom panel)
    ContentBrowserPanel     m_ContentBrowser;
    float                   m_ContentBrowserHeight = 200.0f;

    // ── Editor state ──────────────────────────────────────────────────────────
    float                   m_StepDt       = 1.0f / 60.0f; // dt для Step-режима
};
