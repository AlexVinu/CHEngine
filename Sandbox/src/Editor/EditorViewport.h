#pragma once

#include <CHEngine.h>
#include <functional>

#include "EditorCamera.h"
#include "GizmoSystem.h"
#include "SceneSession.h"

#include <imgui.h>
#include <ImGuizmo.h>

namespace Sandbox {

/// Offscreen viewport: FBO, grid mesh, scene render pass, and ImGui viewport window (image + gizmo).
class EditorViewport
{
public:
    using SessionUpdateFn = std::function<void(SceneSession*, CHEngine::Timestep)>;

    EditorViewport();

    void SetSessionUpdate(SessionUpdateFn session_update) { m_SessionUpdate = std::move(session_update); }

    /// Call once per frame before viewport / gizmo input (ImGuizmo::BeginFrame).
    void Begin();
    /// Optional frame hook; reserved for symmetry with Begin().
    void End();

    /// Renders the active scene into the viewport framebuffer (grid + session simulation tick).
    void Render(SceneSession* scene_session, CHEngine::Timestep dt);

    /// ImGui viewport window: FBO image, gizmo, play/pause border. Updates hover, position, size, FBO resize.
    void DrawImGui(GizmoSystem& gizmo,
                   EditorCamera& editor_camera,
                   SceneSession* scene_session,
                   const ImVec2& vp_pos,
                   const ImVec2& vp_size,
                   ImGuizmo::OPERATION gizmo_operation,
                   ImGuizmo::MODE gizmo_mode);

    CHEngine::ShaderHandle GetMeshShader() const { return m_MeshShader; }

    bool& ShowGrid() { return m_ShowGrid; }
    const ImVec2& GetViewportPos() const { return m_ViewportPos; }
    const ImVec2& GetViewportSize() const { return m_ViewportSize; }
    bool IsViewportHovered() const { return m_ViewportHovered; }

private:
    void BuildGrid();

private:
    CHEngine::ShaderHandle m_MeshShader{};
    CHEngine::ShaderHandle m_GridShader{};
    CHEngine::VertexArrayHandle m_GridVAO{};
    CHEngine::FramebufferHandle m_Framebuffer{};

    ImVec2 m_ViewportPos{ 0.0f, 0.0f };
    ImVec2 m_ViewportSize{ 1280.0f, 720.0f };
    bool m_ViewportHovered = false;

    bool m_ShowGrid = true;

    SessionUpdateFn m_SessionUpdate;
};

} // namespace Sandbox
