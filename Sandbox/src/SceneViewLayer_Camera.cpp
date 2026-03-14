#include "SceneViewLayer.h"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

// ============================================================================
//  Orbit camera
// ============================================================================

void SceneViewLayer::ApplyOrbit()
{
    glm::vec3 fwd = m_Camera.GetForward();
    m_Camera.SetPosition(m_OrbitTarget - fwd * m_OrbitDist);
}

void SceneViewLayer::SetViewPreset(float yaw, float pitch)
{
    m_Camera.SetYaw(yaw);
    m_Camera.SetPitch(pitch);
    ApplyOrbit();
}

void SceneViewLayer::FocusOnSelected()
{
    CHEngine::SceneObject* obj = m_Scene.FindByID(m_SelectedObjectID);
    if (!obj) return;

    m_OrbitTarget = obj->ObjectTransform.Position;

    float maxR = 0.5f;
    for (auto& mesh : obj->Meshes)
        for (auto& v : mesh.GetVertices())
        {
            float d = glm::length(v.Position);
            if (d > maxR) maxR = d;
        }

    float scaleMax = std::max({ obj->ObjectTransform.Scale.x,
                                 obj->ObjectTransform.Scale.y,
                                 obj->ObjectTransform.Scale.z });
    m_OrbitDist = glm::clamp(maxR * scaleMax * 2.5f, 1.0f, 200.0f);
    ApplyOrbit();
}

// ============================================================================
//  Camera input
// ============================================================================

void SceneViewLayer::UpdateCameraInput()
{
    ImGuiIO& io = ImGui::GetIO();

    // Only when cursor is over the 3D viewport and not over gizmo
    if (!m_ViewportHovered || ImGuizmo::IsOver() || io.WantCaptureMouse) return;

    // Two-finger swipe: zoom (Y) + orbit yaw (X)
    if (io.MouseWheel != 0.0f || io.MouseWheelH != 0.0f)
    {
        if (io.KeyCtrl)
        {
            // Ctrl + vertical swipe → orbit pitch
            m_Camera.SetPitch(m_Camera.GetPitch() + io.MouseWheel * 3.0f);
            ApplyOrbit();
        }
        else
        {
            if (io.MouseWheel != 0.0f)
            {
                float factor = 1.0f - io.MouseWheel * 0.12f;
                m_OrbitDist  = glm::clamp(m_OrbitDist * factor, 0.3f, 500.0f);
            }
            if (io.MouseWheelH != 0.0f)
                m_Camera.SetYaw(m_Camera.GetYaw() - io.MouseWheelH * 3.0f);

            ApplyOrbit();
        }
    }

    // RMB drag → pan (grab mode)
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Right, 1.0f))
    {
        float     panScale = m_OrbitDist * 0.0015f;
        glm::vec3 right    = m_Camera.GetRight();
        glm::vec3 up       = m_Camera.GetUp();
        m_OrbitTarget -= right * io.MouseDelta.x * panScale;
        m_OrbitTarget += up    * io.MouseDelta.y * panScale;
        ApplyOrbit();
    }

    // MMB drag → pan (mouse with middle button)
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f))
    {
        float     panScale = m_OrbitDist * 0.0015f;
        glm::vec3 right    = m_Camera.GetRight();
        glm::vec3 up       = m_Camera.GetUp();
        m_OrbitTarget -= right * io.MouseDelta.x * panScale;
        m_OrbitTarget += up    * io.MouseDelta.y * panScale;
        ApplyOrbit();
    }

    // F → frame selected
    if (ImGui::IsKeyPressed(ImGuiKey_F) && m_SelectedObjectID != 0)
        FocusOnSelected();

    // Follow mode
    if (m_FollowObject && m_SelectedObjectID != 0)
    {
        auto* obj = m_Scene.FindByID(m_SelectedObjectID);
        if (obj) { m_OrbitTarget = obj->ObjectTransform.Position; ApplyOrbit(); }
    }
}
