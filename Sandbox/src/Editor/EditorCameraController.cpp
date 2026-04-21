#include "EditorCameraController.h"

#include <CHEngine/Camera/EditorCamera.h>
#include <CHEngine/Scene/Components.h>
#include <CHEngine/Scene/Entity.h>

#include <algorithm>

namespace {
constexpr float k_ZoomFactor = 0.12f;
constexpr float k_OrbitMinDist = 0.3f;
constexpr float k_OrbitMaxDist = 500.0f;
constexpr float k_PanScale = 0.0015f;
constexpr float k_OrbitSens = 0.4f;
constexpr float k_ScrollDeg = 3.0f;
constexpr float k_FocusDistMin = 1.0f;
constexpr float k_FocusDistMax = 200.0f;
constexpr float k_FocusDistScale = 2.5f;
} // namespace

namespace Sandbox {

void EditorCameraController::ApplyOrbit(CHEngine::EditorCamera* viewport_camera, EditorCameraState& camera_state)
{
    if (!viewport_camera)
        return;

    const glm::vec3 forward = viewport_camera->GetForwardDirection();
    viewport_camera->SetPosition(camera_state.OrbitTarget - forward * camera_state.OrbitDist);
}

void EditorCameraController::SetViewPreset(float yaw_degrees,
                                           float pitch_degrees,
                                           CHEngine::EditorCamera* viewport_camera,
                                           EditorCameraState& camera_state)
{
    if (!viewport_camera)
        return;

    viewport_camera->SetYaw(glm::radians(yaw_degrees));
    viewport_camera->SetPitch(glm::radians(pitch_degrees));
    ApplyOrbit(viewport_camera, camera_state);
}

void EditorCameraController::FocusOnPoint(const glm::vec3& target,
                                          float radius,
                                          CHEngine::EditorCamera* viewport_camera,
                                          EditorCameraState& camera_state)
{
    if (!viewport_camera)
        return;

    camera_state.OrbitTarget = target;
    camera_state.OrbitDist = glm::clamp(radius * k_FocusDistScale, k_FocusDistMin, k_FocusDistMax);
    ApplyOrbit(viewport_camera, camera_state);
}

void EditorCameraController::UpdateCameraInput(const InputSnapshot& input_snapshot,
                                               CHEngine::EditorCamera* viewport_camera,
                                               EditorCameraState& camera_state,
                                               CHEngine::Scene* active_scene,
                                               CHEngine::EntityHandle selected_entity)
{
    (void)active_scene;
    (void)selected_entity;

    if (!viewport_camera || !input_snapshot.IsViewportHovered)
        return;

    if (input_snapshot.IsGizmoUsing)
        return;

    if (input_snapshot.MouseWheel != 0.0f)
    {
        if (input_snapshot.IsCtrlPressed)
        {
            viewport_camera->SetPitch(
                viewport_camera->GetPitch() + glm::radians(input_snapshot.MouseWheel * k_ScrollDeg));
        }
        else
        {
            const float factor = 1.0f - input_snapshot.MouseWheel * k_ZoomFactor;
            camera_state.OrbitDist = glm::clamp(camera_state.OrbitDist * factor, k_OrbitMinDist, k_OrbitMaxDist);
        }
        ApplyOrbit(viewport_camera, camera_state);
    }

    const bool isOrbiting = input_snapshot.IsOrbitByRmbDrag
        || input_snapshot.IsOrbitByAltLmbDrag
        || input_snapshot.IsOrbitByMmbDrag;
    if (isOrbiting)
    {
        viewport_camera->SetYaw(
            viewport_camera->GetYaw() + glm::radians(input_snapshot.MouseDelta.x * k_OrbitSens));
        viewport_camera->SetPitch(
            viewport_camera->GetPitch() + glm::radians(input_snapshot.MouseDelta.y * k_OrbitSens));
        ApplyOrbit(viewport_camera, camera_state);
    }

    const bool isPanning = input_snapshot.IsPanByAltShiftLmbDrag
        || input_snapshot.IsPanByShiftMmbDrag
        || input_snapshot.IsPanByShiftRmbDrag;
    if (isPanning)
    {
        const float pan_scale = camera_state.OrbitDist * k_PanScale;
        const glm::vec3 right = viewport_camera->GetRightDirection();
        const glm::vec3 up = viewport_camera->GetUpDirection();
        camera_state.OrbitTarget -= right * input_snapshot.MouseDelta.x * pan_scale;
        camera_state.OrbitTarget += up * input_snapshot.MouseDelta.y * pan_scale;
        ApplyOrbit(viewport_camera, camera_state);
    }
}

void EditorCameraController::OnEvent(CHEngine::Event& e)
{
    (void)e;
}

void EditorCameraController::OnUpdate(CHEngine::EditorCamera* viewport_camera,
                                      EditorCameraState& camera_state,
                                      CHEngine::Scene* active_scene,
                                      CHEngine::EntityHandle selected_entity,
                                      const InputSnapshot& input_snapshot)
{
    if (!camera_state.FollowObject
        || !input_snapshot.IsViewportHovered
        || input_snapshot.IsGizmoUsing
        || !viewport_camera
        || !active_scene
        || !selected_entity.IsValid())
        return;

    CHEngine::Entity* entity = active_scene->TryGetEntity(selected_entity);
    if (!entity || !entity->HasComponent<CHEngine::TransformComponent>())
        return;

    camera_state.OrbitTarget = entity->GetComponent<CHEngine::TransformComponent>().ObjectTransform.Position;
    ApplyOrbit(viewport_camera, camera_state);
}

} // namespace Sandbox
