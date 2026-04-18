#include "EditorCamera.h"

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

void EditorCamera::ApplyOrbit(CHEngine::EditorCamera* viewport_camera)
{
    if (!viewport_camera)
        return;

    const glm::vec3 forward = viewport_camera->GetForwardDirection();
    viewport_camera->SetPosition(m_OrbitTarget - forward * m_OrbitDist);
}

void EditorCamera::SetViewPreset(float yaw_degrees, float pitch_degrees, CHEngine::EditorCamera* viewport_camera)
{
    if (!viewport_camera)
        return;

    viewport_camera->SetYaw(glm::radians(yaw_degrees));
    viewport_camera->SetPitch(glm::radians(pitch_degrees));
    ApplyOrbit(viewport_camera);
}

void EditorCamera::FocusOnSelected(CHEngine::EditorCamera* viewport_camera,
                                   CHEngine::Scene* active_scene,
                                   CHEngine::EntityHandle selected_entity)
{
    if (!viewport_camera || !active_scene)
        return;

    auto* entity = active_scene->TryGetEntity(selected_entity);
    if (!entity
        || !entity->HasComponent<CHEngine::TransformComponent>()
        || !entity->HasComponent<CHEngine::MeshComponent>())
        return;

    auto& transform = entity->GetComponent<CHEngine::TransformComponent>().ObjectTransform;
    auto& meshes = entity->GetComponent<CHEngine::MeshComponent>().Meshes;
    m_OrbitTarget = transform.Position;

    float maxRadius = 0.5f;
    for (const auto& mesh : meshes)
    {
        for (const auto& vertex : mesh.GetVertices())
        {
            const float distance = glm::length(vertex.Position);
            if (distance > maxRadius)
                maxRadius = distance;
        }
    }

    const float scaleMax = std::max({ transform.Scale.x, transform.Scale.y, transform.Scale.z });
    m_OrbitDist = glm::clamp(maxRadius * scaleMax * k_FocusDistScale, k_FocusDistMin, k_FocusDistMax);
    ApplyOrbit(viewport_camera);
}

void EditorCamera::UpdateCameraInput(const InputSnapshot& input_snapshot,
                                     CHEngine::EditorCamera* viewport_camera,
                                     CHEngine::Scene* active_scene,
                                     CHEngine::EntityHandle selected_entity)
{
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
            m_OrbitDist = glm::clamp(m_OrbitDist * factor, k_OrbitMinDist, k_OrbitMaxDist);
        }
        ApplyOrbit(viewport_camera);
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
        ApplyOrbit(viewport_camera);
    }

    const bool isPanning = input_snapshot.IsPanByAltShiftLmbDrag
        || input_snapshot.IsPanByShiftMmbDrag
        || input_snapshot.IsPanByShiftRmbDrag;
    if (isPanning)
    {
        const float panScale = m_OrbitDist * k_PanScale;
        const glm::vec3 right = viewport_camera->GetRightDirection();
        const glm::vec3 up = viewport_camera->GetUpDirection();
        m_OrbitTarget -= right * input_snapshot.MouseDelta.x * panScale;
        m_OrbitTarget += up * input_snapshot.MouseDelta.y * panScale;
        ApplyOrbit(viewport_camera);
    }

    if (input_snapshot.IsFocusPressed && selected_entity.IsValid())
        FocusOnSelected(viewport_camera, active_scene, selected_entity);
}

void EditorCamera::OnEvent(CHEngine::Event& e)
{
    (void)e;
}

void EditorCamera::OnUpdate(CHEngine::EditorCamera* viewport_camera,
                            CHEngine::Scene* active_scene,
                            CHEngine::EntityHandle selected_entity,
                            bool is_viewport_hovered,
                            bool is_gizmo_using)
{
    if (!m_FollowObject
        || !is_viewport_hovered
        || is_gizmo_using
        || !viewport_camera
        || !active_scene
        || !selected_entity.IsValid())
        return;

    auto* entity = active_scene->TryGetEntity(selected_entity);
    if (!entity || !entity->HasComponent<CHEngine::TransformComponent>())
        return;

    m_OrbitTarget = entity->GetComponent<CHEngine::TransformComponent>().ObjectTransform.Position;
    ApplyOrbit(viewport_camera);
}

} // namespace Sandbox
