#include "SceneViewLayer.h"

#include <CHEngine/Input/Input.h>
#include <Input/KeyCodes.h>

// ── Camera tuning constants ─────────────────────────────────────────────────
namespace CamCfg {
    constexpr float ZoomFactor      = 0.12f;   // scroll → distance multiplier
    constexpr float OrbitMinDist    = 0.3f;
    constexpr float OrbitMaxDist    = 500.0f;
    constexpr float PanScale        = 0.0015f; // mouse-delta → world-units
    constexpr float FlySpeed        = 0.05f;   // base fly speed (units/frame)
    constexpr float FlyShiftMult    = 4.0f;    // shift speedup
    constexpr float OrbitSens       = 0.4f;    // orbit drag sensitivity
    constexpr float LookSens        = 0.3f;    // RMB look sensitivity
    constexpr float ScrollDeg       = 3.0f;    // scroll → yaw/pitch degrees
    constexpr float FocusDistMin    = 1.0f;
    constexpr float FocusDistMax    = 200.0f;
    constexpr float FocusDistScale  = 2.5f;
}

// ============================================================================
//  Orbit camera helpers
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
    m_OrbitDist = glm::clamp(maxR * scaleMax * CamCfg::FocusDistScale,
                             CamCfg::FocusDistMin, CamCfg::FocusDistMax);
    ApplyOrbit();
}

// ============================================================================
//  Camera input
// ============================================================================

void SceneViewLayer::UpdateCameraInput()
{
    ImGuiIO& io = ImGui::GetIO();

    if (!m_ViewportHovered) return;

    // Блокируем только когда gizmo активно перетаскивается
    if (ImGuizmo::IsUsing()) return;

    // ── Scroll: zoom + yaw pan ───────────────────────────────────────────────
    if (io.MouseWheel != 0.0f || io.MouseWheelH != 0.0f)
    {
        if (io.KeyCtrl)
        {
            m_Camera.SetPitch(m_Camera.GetPitch() + io.MouseWheel * CamCfg::ScrollDeg);
        }
        else
        {
            if (io.MouseWheel != 0.0f)
            {
                float factor = 1.0f - io.MouseWheel * CamCfg::ZoomFactor;
                m_OrbitDist  = glm::clamp(m_OrbitDist * factor,
                                          CamCfg::OrbitMinDist, CamCfg::OrbitMaxDist);
            }
            if (io.MouseWheelH != 0.0f)
                m_Camera.SetYaw(m_Camera.GetYaw() - io.MouseWheelH * CamCfg::ScrollDeg);
        }
        ApplyOrbit();
    }

    // ── Orbit: Alt+LMB drag  OR  MMB drag ────────────────────────────────────
    bool orbitByAltLMB = io.KeyAlt && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 1.0f);
    bool orbitByMMB    = !io.KeyShift && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f);
    if (orbitByAltLMB || orbitByMMB)
    {
        m_Camera.SetYaw  (m_Camera.GetYaw()   - io.MouseDelta.x * CamCfg::OrbitSens);
        m_Camera.SetPitch(m_Camera.GetPitch() + io.MouseDelta.y * CamCfg::OrbitSens);
        ApplyOrbit();
    }

    // ── Pan: RMB drag  OR  Shift+MMB drag ────────────────────────────────────
    bool panByRMB      = ImGui::IsMouseDragging(ImGuiMouseButton_Right, 1.0f);
    bool panByShiftMMB = io.KeyShift && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f);
    if (panByRMB || panByShiftMMB)
    {
        float     panScale = m_OrbitDist * CamCfg::PanScale;
        glm::vec3 right    = m_Camera.GetRight();
        glm::vec3 up       = m_Camera.GetUp();
        m_OrbitTarget -= right * io.MouseDelta.x * panScale;
        m_OrbitTarget += up    * io.MouseDelta.y * panScale;
        ApplyOrbit();
    }

    // ── RMB held + WASD = Blender-style fly (без OS-задержки через Input) ───
    if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
    {
        float speed = CamCfg::FlySpeed * m_OrbitDist;
        if (CHEngine::Input::IsKeyDown(CHEngine::Key::LeftShift) ||
            CHEngine::Input::IsKeyDown(CHEngine::Key::RightShift))
            speed *= CamCfg::FlyShiftMult;

        glm::vec3 fwd   = m_Camera.GetForward();
        glm::vec3 right = m_Camera.GetRight();
        glm::vec3 up    = m_Camera.GetUp();
        glm::vec3 move  = glm::vec3(0.0f);

        if (CHEngine::Input::IsKeyDown(CHEngine::Key::W)) move += fwd   * speed;
        if (CHEngine::Input::IsKeyDown(CHEngine::Key::S)) move -= fwd   * speed;
        if (CHEngine::Input::IsKeyDown(CHEngine::Key::A)) move -= right * speed;
        if (CHEngine::Input::IsKeyDown(CHEngine::Key::D)) move += right * speed;
        if (CHEngine::Input::IsKeyDown(CHEngine::Key::E)) move += up    * speed;
        if (CHEngine::Input::IsKeyDown(CHEngine::Key::Q)) move -= up    * speed;

        if (glm::length(move) > 0.0001f)
        {
            m_OrbitTarget += move;
            ApplyOrbit();
        }

        // RMB + drag → look around (вместо pan)
        if (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f)
        {
            m_Camera.SetYaw  (m_Camera.GetYaw()   - io.MouseDelta.x * CamCfg::LookSens);
            m_Camera.SetPitch(m_Camera.GetPitch() + io.MouseDelta.y * CamCfg::LookSens);
            ApplyOrbit();
        }
    }

    // ── F → frame selected ───────────────────────────────────────────────────
    if (CHEngine::Input::IsKeyPressed(CHEngine::Key::F) && m_SelectedObjectID != 0)
        FocusOnSelected();

    // ── Follow mode ──────────────────────────────────────────────────────────
    if (m_FollowObject && m_SelectedObjectID != 0)
    {
        auto* obj = m_Scene.FindByID(m_SelectedObjectID);
        if (obj) { m_OrbitTarget = obj->ObjectTransform.Position; ApplyOrbit(); }
    }
}
