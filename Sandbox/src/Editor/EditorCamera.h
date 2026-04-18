#pragma once

#include <CHEngine/Scene/Scene.h>
#include <glm/glm.hpp>

namespace CHEngine {
class EditorCamera;
class Event;
}

namespace Sandbox {

class EditorCamera
{
public:
    struct InputSnapshot
    {
        bool IsViewportHovered = false;
        bool IsGizmoUsing = false;

        bool IsCtrlPressed = false;
        bool IsAltPressed = false;
        bool IsShiftPressed = false;
        bool IsFocusPressed = false;

        float MouseWheel = 0.0f;
        glm::vec2 MouseDelta{ 0.0f, 0.0f };

        bool IsOrbitByRmbDrag = false;
        bool IsOrbitByAltLmbDrag = false;
        bool IsOrbitByMmbDrag = false;

        bool IsPanByAltShiftLmbDrag = false;
        bool IsPanByShiftMmbDrag = false;
        bool IsPanByShiftRmbDrag = false;
    };

public:
    void ApplyOrbit(CHEngine::EditorCamera* viewport_camera);
    void UpdateCameraInput(const InputSnapshot& input_snapshot,
                           CHEngine::EditorCamera* viewport_camera,
                           CHEngine::Scene* active_scene,
                           CHEngine::EntityHandle selected_entity);
    void SetViewPreset(float yaw_degrees, float pitch_degrees, CHEngine::EditorCamera* viewport_camera);
    void FocusOnSelected(CHEngine::EditorCamera* viewport_camera,
                         CHEngine::Scene* active_scene,
                         CHEngine::EntityHandle selected_entity);
    void OnEvent(CHEngine::Event& e);
    void OnUpdate(CHEngine::EditorCamera* viewport_camera,
                  CHEngine::Scene* active_scene,
                  CHEngine::EntityHandle selected_entity,
                  bool is_viewport_hovered,
                  bool is_gizmo_using);

    const glm::vec3& GetOrbitTarget() const { return m_OrbitTarget; }
    void SetOrbitTarget(const glm::vec3& orbit_target) { m_OrbitTarget = orbit_target; }

    float GetOrbitDist() const { return m_OrbitDist; }
    void SetOrbitDist(float orbit_dist) { m_OrbitDist = orbit_dist; }

    float GetAspectRatio() const { return m_AspectRatio; }
    void SetAspectRatio(float aspect_ratio) { m_AspectRatio = aspect_ratio; }

    bool GetFollowObject() const { return m_FollowObject; }
    void SetFollowObject(bool follow_object) { m_FollowObject = follow_object; }

private:
    glm::vec3 m_OrbitTarget{ 0.0f, 0.0f, 0.0f };
    float m_OrbitDist = 8.0f;
    float m_AspectRatio = 1280.0f / 720.0f;
    bool m_FollowObject = false;
};

} // namespace Sandbox
