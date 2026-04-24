#include "GizmoSystem.h"

#include <CHEngine/Camera/EditorCamera.h>
#include <CHEngine/Scene/Components.h>
#include <CHEngine/Scene/Entity.h>

#include <glm/gtc/type_ptr.hpp>

namespace Sandbox {

void GizmoSystem::OnEvent(CHEngine::Event& /*e*/)
{
}

void GizmoSystem::TransformToMatrix(const CHEngine::Transform& transform, float out_matrix[16])
{
    float translation[3] = { transform.Position.x, transform.Position.y, transform.Position.z };
    float rotation[3] = { transform.Rotation.x, transform.Rotation.y, transform.Rotation.z };
    float scale[3] = { transform.Scale.x, transform.Scale.y, transform.Scale.z };
    ImGuizmo::RecomposeMatrixFromComponents(translation, rotation, scale, out_matrix);
}

void GizmoSystem::Draw(SceneSession* scene_session,
                       ImGuizmo::OPERATION gizmo_operation,
                       ImGuizmo::MODE gizmo_mode,
                       const ImVec2& viewport_pos,
                       const ImVec2& viewport_size)
{
    if (!scene_session)
        return;

    if (scene_session->SessionState != SceneSession::State::Edit)
    {
        m_GizmoWasUsing = false;
        return;
    }

    if (!scene_session->ViewportCamera || !scene_session->EditorScene)
        return;

    auto* viewport_camera = scene_session->ViewportCamera.get();
    auto scene_ref = scene_session->EditorScene;
    if (!viewport_camera || !scene_ref)
        return;
    const CHEngine::EntityHandle selectedHandle = scene_session->SelectedEntity;
    if (!scene_ref->IsEntityHandleValid(selectedHandle))
        return;

    auto* entity = scene_ref->TryGetEntity(selectedHandle);
    if (!entity || !entity->HasComponent<CHEngine::TransformComponent>())
        return;

    auto& selectedTransform = entity->GetComponent<CHEngine::TransformComponent>().ObjectTransform;

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();
    ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::SetRect(viewport_pos.x, viewport_pos.y, viewport_size.x, viewport_size.y);

    glm::mat4 view = viewport_camera->GetViewMatrix();
    glm::mat4 proj = viewport_camera->GetProjectionMatrix();

    float translation[3];
    float rotation[3];
    float scale[3];
    float matrix[16];
    TransformToMatrix(selectedTransform, matrix);

    float snapTranslation[3] = { 1.0f, 1.0f, 1.0f };
    float snapRotation[3] = { 45.0f, 45.0f, 45.0f };
    float snapScale[3] = { 0.1f, 0.1f, 0.1f };
    float* snap = nullptr;
    if (io.KeyShift)
    {
        if (gizmo_operation == ImGuizmo::TRANSLATE)
            snap = snapTranslation;
        else if (gizmo_operation == ImGuizmo::ROTATE)
            snap = snapRotation;
        else if (gizmo_operation == ImGuizmo::SCALE)
            snap = snapScale;
    }

    if (!m_GizmoWasUsing && ImGuizmo::IsUsing())
        m_TransformBeforeDrag = selectedTransform;

    ImGuizmo::Manipulate(glm::value_ptr(view),
                         glm::value_ptr(proj),
                         gizmo_operation,
                         gizmo_mode,
                         matrix,
                         nullptr,
                         snap);

    if (ImGuizmo::IsUsing())
    {
        ImGuizmo::DecomposeMatrixToComponents(matrix, translation, rotation, scale);
        selectedTransform.Position = { translation[0], translation[1], translation[2] };
        selectedTransform.Rotation = { rotation[0], rotation[1], rotation[2] };
        selectedTransform.Scale = { scale[0], scale[1], scale[2] };
    }

    if (m_GizmoWasUsing && !ImGuizmo::IsUsing() && m_CommandStack)
    {
        auto command = CHEngine::MakeScope<SetTransformCommand>(
            scene_ref,
            selectedHandle,
            m_TransformBeforeDrag,
            selectedTransform);
        m_CommandStack->Push(std::move(command));
    }

    m_GizmoWasUsing = ImGuizmo::IsUsing();
}

} // namespace Sandbox
