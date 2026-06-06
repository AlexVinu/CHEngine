#pragma once

#include "SceneSession.h"
#include "SetTransformCommand.h"

#include "CHEngine/Events/Event.h"

#include <ImGuizmo.h>
#include <imgui.h>

namespace Sandbox {

class GizmoSystem
{
public:
    GizmoSystem() = default;

    void BindCommandStack(CommandStack* command_stack) { m_CommandStack = command_stack; }

    void Draw(SceneSession* scene_session,
              ImGuizmo::OPERATION gizmo_operation,
              ImGuizmo::MODE gizmo_mode,
              const ImVec2& viewport_pos,
              const ImVec2& viewport_size);

private:
    static void TransformToMatrix(const CHEngine::Transform& transform, float out_matrix[16]);

private:
    CommandStack* m_CommandStack = nullptr;
    CHEngine::Transform m_TransformBeforeDrag{};
    bool m_GizmoWasUsing = false;
};

} // namespace Sandbox
