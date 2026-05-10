#pragma once

#include <CHEngine.h>

#include "EditorCameraController.h"
#include "GizmoSystem.h"
#include "SceneSession.h"

#include <imgui.h>
#include <ImGuizmo.h>

namespace Sandbox {

/// Offscreen viewport: FBO, grid mesh, scene render pass, and ImGui viewport window (image + gizmo).
class EditorViewport
{
public:
    EditorViewport();

    /// Call once per frame before viewport / gizmo input (ImGuizmo::BeginFrame).
    void Begin();
    /// Optional frame hook; reserved for symmetry with Begin().
    void End();

    /// Starts scene rendering into the viewport framebuffer (bind + clear + camera setup).
    void BeginSceneRender(SceneSession* scene_session);
    /// Registers editor-only render passes (grid) into the frame graph.
    /// Must be called AFTER RenderSystem has registered MainColor + Tonemap passes
    /// so the grid pass writes to the LDR viewport-output target.
    void RegisterEditorPasses(SceneSession* scene_session);
    /// Completes scene rendering into the viewport framebuffer (unbind).
    void EndSceneRender();

    /// ImGui viewport window: FBO image, gizmo, play/pause border. Updates hover, position, size, FBO resize.
    void DrawImGui(GizmoSystem& gizmo,
                   EditorCameraController& camera_controller,
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
    CHEngine::ShaderHandle   m_GridShader{};
    CHEngine::PipelineHandle m_GridPipeline{};
    // Grid mesh buffers — fullscreen quad in NDC space.
    CHEngine::BufferHandle m_GridVB{};
    CHEngine::BufferHandle m_GridIB{};
    CHEngine::BufferHandle m_GridCameraUBO{}; // camera UBO for grid shader
    uint32_t               m_GridIndexCount = 0;
    // Viewport output texture is owned by the frame graph and read via
    // RenderFacade::GetViewportColorTexID() for ImGui::Image.

    ImVec2 m_ViewportPos{ 0.0f, 0.0f };
    ImVec2 m_ViewportSize{ 1280.0f, 720.0f };
    bool m_ViewportHovered = false;

    bool m_ShowGrid = true;
};

} // namespace Sandbox
