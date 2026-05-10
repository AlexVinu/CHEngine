#pragma once

#include "EditorCameraState.h"

#include <CHEngine/Scene/Scene.h>
#include <glm/glm.hpp>

namespace CHEngine {
class EditorCamera;
class Event;
}

namespace Sandbox {

class EditorCameraController
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
    void ApplyOrbit(CHEngine::EditorCamera* viewport_camera, EditorCameraState& camera_state);
    void UpdateCameraInput(const InputSnapshot& input_snapshot,
                           CHEngine::EditorCamera* viewport_camera,
                           EditorCameraState& camera_state,
                           CHEngine::Ref<CHEngine::Scene> editor_scene,
                           CHEngine::EntityHandle selected_entity);
    void SetViewPreset(float yaw_degrees,
                       float pitch_degrees,
                       CHEngine::EditorCamera* viewport_camera,
                       EditorCameraState& camera_state);
    void FocusOnPoint(const glm::vec3& target,
                      float radius,
                      CHEngine::EditorCamera* viewport_camera,
                      EditorCameraState& camera_state);
    void OnEvent(CHEngine::Event& e);
    void OnUpdate(CHEngine::EditorCamera* viewport_camera,
                  EditorCameraState& camera_state,
                  CHEngine::Ref<CHEngine::Scene> editor_scene,
                  CHEngine::EntityHandle selected_entity,
                  const InputSnapshot& input_snapshot);

    float GetAspectRatio() const { return m_AspectRatio; }
    void SetAspectRatio(float aspect_ratio) { m_AspectRatio = aspect_ratio; }

private:
    float m_AspectRatio = 1280.0f / 720.0f;
};

} // namespace Sandbox
