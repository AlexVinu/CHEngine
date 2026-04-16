#include "SceneViewLayer.h"

#include <CHEngine/Input/Input.h>
#include <Input/KeyCodes.h>

// ── Camera tuning constants ─────────────────────────────────────────────────
namespace CamCfg {
    constexpr float ZoomFactor      = 0.12f;   // scroll → distance multiplier
    constexpr float OrbitMinDist    = 0.3f;
    constexpr float OrbitMaxDist    = 500.0f;
    constexpr float PanScale        = 0.0015f; // mouse-delta → world-units
    constexpr float OrbitSens       = 0.4f;    // orbit drag sensitivity
    constexpr float ScrollDeg       = 3.0f;    // Ctrl+scroll → pitch degrees
    constexpr float FocusDistMin    = 1.0f;
    constexpr float FocusDistMax    = 200.0f;
    constexpr float FocusDistScale  = 2.5f;
}

// ============================================================================
//  Orbit camera helpers
// ============================================================================

void SceneViewLayer::ApplyOrbit()
{
    CHEngine::EditorCamera& viewportCamera = *GetActiveSceneSession().ViewportCamera;
    glm::vec3 fwd = viewportCamera.GetForwardDirection();
    viewportCamera.SetPosition(m_OrbitTarget - fwd * m_OrbitDist);
}

void SceneViewLayer::SetViewPreset(float yaw, float pitch)
{
    CHEngine::EditorCamera& viewportCamera = *GetActiveSceneSession().ViewportCamera;
    viewportCamera.SetYaw(glm::radians(yaw));
    viewportCamera.SetPitch(glm::radians(pitch));
    ApplyOrbit();
}

void SceneViewLayer::FocusOnSelected()
{
    auto scene = GetActiveSceneSession().ActiveScene;
    auto selected_handle = GetActiveSceneSession().SelectedEntity;
    auto* entity = scene->TryGetEntity(selected_handle);
    if (!entity
        || !entity->HasComponent<CHEngine::TransformComponent>()
        || !entity->HasComponent<CHEngine::MeshComponent>()) return;
    auto& transform = entity->GetComponent<CHEngine::TransformComponent>().ObjectTransform;
    auto& meshes = entity->GetComponent<CHEngine::MeshComponent>().Meshes;
    m_OrbitTarget = transform.Position;

    float maxR = 0.5f;
    for (auto& mesh : meshes)
        for (auto& v : mesh.GetVertices())
        {
            float d = glm::length(v.Position);
            if (d > maxR) maxR = d;
        }

    float scaleMax = std::max({ transform.Scale.x,
                                 transform.Scale.y,
                                 transform.Scale.z });
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
    CHEngine::EditorCamera& viewportCamera = *GetActiveSceneSession().ViewportCamera;

    if (!m_ViewportHovered) return;

    // Блокируем только когда gizmo активно перетаскивается
    if (ImGuizmo::IsUsing()) return;

    // ── Scroll: zoom (Blender: Ctrl+Scroll → pitch) ──────────────────────────
    if (io.MouseWheel != 0.0f)
    {
        if (io.KeyCtrl)
        {
            // Ctrl + прокрутка → наклон камеры
            viewportCamera.SetPitch(viewportCamera.GetPitch() + glm::radians(io.MouseWheel * CamCfg::ScrollDeg));
        }
        else
        {
            // Обычная прокрутка → зум (как в Blender)
            float factor = 1.0f - io.MouseWheel * CamCfg::ZoomFactor;
            m_OrbitDist  = glm::clamp(m_OrbitDist * factor,
                                      CamCfg::OrbitMinDist, CamCfg::OrbitMaxDist);
        }
        ApplyOrbit();
    }

    // ── Orbit: RMB drag  OR  Alt+LMB drag  OR  MMB drag ─────────────────────
    //    (Blender: ПКМ / два пальца по тачпаду / MMB = вращение)
    bool orbitByRMB    = ImGui::IsMouseDragging(ImGuiMouseButton_Right,  1.0f);
    bool orbitByAltLMB = io.KeyAlt && !io.KeyShift &&
                         ImGui::IsMouseDragging(ImGuiMouseButton_Left,   1.0f);
    bool orbitByMMB    = !io.KeyShift &&
                         ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f);

    if (orbitByRMB || orbitByAltLMB || orbitByMMB)
    {
        viewportCamera.SetYaw  (viewportCamera.GetYaw()   - glm::radians(io.MouseDelta.x * CamCfg::OrbitSens));
        viewportCamera.SetPitch(viewportCamera.GetPitch() + glm::radians(io.MouseDelta.y * CamCfg::OrbitSens));
        ApplyOrbit();
    }

    // ── Pan: Alt+Shift+LMB drag  OR  Shift+MMB drag ──────────────────────────
    //    (Blender: Shift+ПКМ / Shift+два пальца = рука, перемещение)
    bool panByAltShift = io.KeyAlt && io.KeyShift &&
                         ImGui::IsMouseDragging(ImGuiMouseButton_Left,   1.0f);
    bool panByShiftMMB = io.KeyShift &&
                         ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f);
    bool panByShiftRMB = io.KeyShift && !io.KeyAlt &&
                         ImGui::IsMouseDragging(ImGuiMouseButton_Right,  1.0f);

    if (panByAltShift || panByShiftMMB || panByShiftRMB)
    {
        float     panScale = m_OrbitDist * CamCfg::PanScale;
        glm::vec3 right    = viewportCamera.GetRightDirection();
        glm::vec3 up       = viewportCamera.GetUpDirection();
        m_OrbitTarget -= right * io.MouseDelta.x * panScale;
        m_OrbitTarget += up    * io.MouseDelta.y * panScale;
        ApplyOrbit();
    }

    // ── F → frame selected ───────────────────────────────────────────────────
    if (CHEngine::Input::IsKeyPressed(CHEngine::Key::F) && GetActiveSceneSession().SelectedEntity.IsValid())
        FocusOnSelected();

    // ── Follow mode ──────────────────────────────────────────────────────────
    if (m_FollowObject && GetActiveSceneSession().SelectedEntity.IsValid())
    {
        auto scene = GetActiveSceneSession().ActiveScene;
        auto handle = GetActiveSceneSession().SelectedEntity;
        if (auto* entity = scene->TryGetEntity(handle); entity && entity->HasComponent<CHEngine::TransformComponent>()) {
            m_OrbitTarget = entity->GetComponent<CHEngine::TransformComponent>().ObjectTransform.Position;
            ApplyOrbit();
        }
    }
}
