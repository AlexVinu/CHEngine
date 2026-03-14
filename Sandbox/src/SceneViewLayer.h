#pragma once

#include <CHEngine.h>
#include "UIThemeActive.h"
#include "UndoStack.h"
#include "ContentBrowserPanel.h"

#include <glm/glm.hpp>
#include <chrono>
#include <string>

// ============================================================================
//  SceneViewLayer — main 3D editor layer for Sandbox
//
//  Panels
//    Toolbar     — gizmo mode, grid, snap, hotkeys (T/R/S)
//    Scene       — object hierarchy, import button
//    Properties  — transform, material, mesh info
//    Camera      — view presets, orbit controls
//
//  Camera controls (macOS trackpad)
//    Two-finger swipe up/down   → zoom
//    Two-finger swipe left/right→ orbit yaw
//    Ctrl + swipe up/down       → orbit pitch
//    RMB drag                   → pan (grab mode)
//    MMB drag                   → pan
//    F                          → frame selected
//    Shift + gizmo drag         → snap (1 unit / 45° / 0.1)
// ============================================================================
class SceneViewLayer : public CHEngine::Layer
{
public:
    SceneViewLayer();

    void OnUpdate()      override;
    void OnImGuiRender() override;
    void OnEvent(CHEngine::Event& e) override {}

private:
    // ── Orbit camera ─────────────────────────────────────────────────────────
    void ApplyOrbit();
    void SetViewPreset(float yaw, float pitch);
    void FocusOnSelected();
    void UpdateCameraInput();

    // ── Grid / axes geometry ─────────────────────────────────────────────────
    void BuildGrid();

    // ── Rendering ────────────────────────────────────────────────────────────
    void RenderScene();

    // ── UI panels ────────────────────────────────────────────────────────────
    void DrawToolbar      (ImVec2 pos, ImVec2 size);
    void DrawScenePanel   (ImVec2 pos, ImVec2 size, bool resetSize);
    void DrawPropsPanel   (ImVec2 pos, ImVec2 size, bool resetSize);
    void DrawCameraPanel  (ImVec2 pos, ImVec2 size, bool resetSize);
    void DrawOrbitIndicator();
    void DrawGizmo();

    // ── Model import ─────────────────────────────────────────────────────────
    void ImportModel(const std::string& filepath);

    // ── Scene serialization ───────────────────────────────────────────────────
    void SaveScene();
    void LoadScene(const std::string& path = "");

    // =========================================================================
    // State
    // =========================================================================

    // Scene & render resources
    CHEngine::Scene             m_Scene;
    CHEngine::Camera            m_Camera;
    CHEngine::ShaderHandle      m_MeshShader;
    CHEngine::ShaderHandle      m_GridShader;
    CHEngine::VertexArrayHandle m_GridVAO;
    CHEngine::RenderAPIHandle   m_RenderApi;

    // Orbit camera
    glm::vec3 m_OrbitTarget  = { 0.0f, 0.0f, 0.0f };
    float     m_OrbitDist    = 8.0f;
    bool      m_FollowObject = false;

    // Selection & viewport
    uint32_t  m_SelectedObjectID = 0;
    float     m_AspectRatio      = 16.0f / 9.0f;

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
    bool m_ShowGrid    = true;
    bool m_ResetLayout = false;   // true → snap panel sizes back next frame

    // Delta-time
    std::chrono::steady_clock::time_point m_LastTime;

    // Undo
    UndoStack               m_UndoStack;
    CHEngine::Transform     m_TransformBeforeDrag;
    bool                    m_GizmoWasUsing = false;

    // Recent files
    CHEngine::RecentFiles   m_RecentFiles;

    // Content Browser (bottom panel)
    ContentBrowserPanel     m_ContentBrowser;
    float                   m_ContentBrowserHeight = 200.0f;
};
