#include "SceneViewLayer_CameraOps.h"

#include "SceneViewLayer.h"
#include "SceneViewLayerAccess.h"

#include <CHEngine/Input/Input.h>
#include <Input/KeyCodes.h>
#include <CHEngine/Scene/Components.h>
#include <CHEngine/Scene/Entity.h>

#include <imgui.h>
#include <ImGuizmo.h>

#include <algorithm>

namespace SceneViewLayerCameraOps {

void ApplyOrbit(SceneViewLayer& layer)
{
    Ref<EditorWorldContext> ctx = SceneViewLayerAccess::ActiveRef(layer);
    SceneViewLayerAccess::CameraController(layer).ApplyOrbit(ctx->ViewportCamera.get(), ctx->EditorCameraState);
}

void SetViewPreset(SceneViewLayer& layer, float yaw_degrees, float pitch_degrees)
{
    Ref<EditorWorldContext> ctx = SceneViewLayerAccess::ActiveRef(layer);
    SceneViewLayerAccess::CameraController(layer).SetViewPreset(
        yaw_degrees, pitch_degrees, ctx->ViewportCamera.get(), ctx->EditorCameraState);
}

void FocusOnSelected(SceneViewLayer& layer)
{
    constexpr float k_DefaultFocusRadius = 0.5f;

    Ref<EditorWorldContext> ctx = SceneViewLayerAccess::ActiveRef(layer);
    auto active_scene = ctx->EditorScene;
    CHEngine::Entity* entity = active_scene ? active_scene->TryGetEntity(ctx->SelectedEntity) : nullptr;
    if (!entity
        || !entity->HasComponent<CHEngine::TransformComponent>()
        || !entity->HasComponent<CHEngine::MeshComponent>())
        return;

    const auto& transform = entity->GetComponent<CHEngine::TransformComponent>().ObjectTransform;
    const auto& meshes = entity->GetComponent<CHEngine::MeshComponent>().Meshes;

    float max_radius = k_DefaultFocusRadius;
    for (const auto& mesh : meshes)
    {
        for (const auto& vertex : mesh.GetVertices())
        {
            const float distance = glm::length(vertex.Position);
            if (distance > max_radius)
                max_radius = distance;
        }
    }

    const float scale_max = std::max({ transform.Scale.x, transform.Scale.y, transform.Scale.z });
    SceneViewLayerAccess::CameraController(layer).FocusOnPoint(
        transform.Position, max_radius * scale_max, ctx->ViewportCamera.get(), ctx->EditorCameraState);
}

void UpdateEditorCameraInput(SceneViewLayer& layer)
{
    ImGuiIO& io = ImGui::GetIO();

    Sandbox::EditorCameraController::InputSnapshot inputSnapshot{};
    inputSnapshot.IsViewportHovered = SceneViewLayerAccess::Viewport(layer).IsViewportHovered();
    inputSnapshot.IsGizmoUsing = ImGuizmo::IsUsing();
    inputSnapshot.IsCtrlPressed = io.KeyCtrl;
    inputSnapshot.IsAltPressed = io.KeyAlt;
    inputSnapshot.IsShiftPressed = io.KeyShift;
    inputSnapshot.IsFocusPressed = CHEngine::Input::IsKeyPressed(CHEngine::Key::F);
    inputSnapshot.MouseWheel = io.MouseWheel;
    inputSnapshot.MouseDelta = { io.MouseDelta.x, io.MouseDelta.y };

    inputSnapshot.IsOrbitByRmbDrag = ImGui::IsMouseDragging(ImGuiMouseButton_Right, 1.0f);
    inputSnapshot.IsOrbitByAltLmbDrag = io.KeyAlt && !io.KeyShift
        && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 1.0f);
    inputSnapshot.IsOrbitByMmbDrag = !io.KeyShift
        && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f);

    inputSnapshot.IsPanByAltShiftLmbDrag = io.KeyAlt && io.KeyShift
        && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 1.0f);
    inputSnapshot.IsPanByShiftMmbDrag = io.KeyShift
        && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f);
    inputSnapshot.IsPanByShiftRmbDrag = io.KeyShift && !io.KeyAlt
        && ImGui::IsMouseDragging(ImGuiMouseButton_Right, 1.0f);

    Ref<EditorWorldContext> ctx = SceneViewLayerAccess::ActiveRef(layer);
    SceneViewLayerAccess::CameraController(layer).UpdateCameraInput(
        inputSnapshot, ctx->ViewportCamera.get(), ctx->EditorCameraState, ctx->EditorScene, ctx->SelectedEntity);

    if (inputSnapshot.IsFocusPressed && ctx->SelectedEntity.IsValid())
        FocusOnSelected(layer);

    SceneViewLayerAccess::CameraController(layer).OnUpdate(
        ctx->ViewportCamera.get(), ctx->EditorCameraState, ctx->EditorScene, ctx->SelectedEntity, inputSnapshot);
}

void PrepareEditorCameraFrame(SceneViewLayer& layer)
{
    Ref<EditorWorldContext> ctx = SceneViewLayerAccess::ActiveRef(layer);
    // In Play the world uses scene cameras only; editor orbit camera must not move or consume input.
    if (ctx->GetSessionState() == SceneSession::State::Play)
        return;

    UpdateEditorCameraInput(layer);
}

} // namespace SceneViewLayerCameraOps

namespace SceneViewLayerRender {

void DrawOrbitIndicator(SceneViewLayer& layer)
{
    Ref<EditorWorldContext> ctx = SceneViewLayerAccess::ActiveRef(layer);
    if (ctx->GetSessionState() == SceneSession::State::Play || ctx->GetSessionState() == SceneSession::State::Pause)
        return;

    ImGuiIO& io = ImGui::GetIO();
    const float W = io.DisplaySize.x;
    const float H = io.DisplaySize.y;

    auto* viewport_camera = ctx->ViewportCamera.get();
    if (!viewport_camera)
        return;
    glm::mat4 view = viewport_camera->GetViewMatrix();
    glm::mat4 proj = viewport_camera->GetProjectionMatrix();

    glm::vec4 clip = proj * view * glm::vec4(ctx->EditorCameraState.OrbitTarget, 1.0f);
    if (clip.w <= 0.0f)
        return;

    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    if (glm::abs(ndc.x) > 1.0f || glm::abs(ndc.y) > 1.0f)
        return;

    float sx = (ndc.x + 1.0f) * 0.5f * W;
    float sy = (1.0f - ndc.y) * 0.5f * H;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const float r = 6.0f;
    const float gap = 2.5f;
    const ImU32 col = IM_COL32(255, 255, 255, 140);
    const ImU32 out = IM_COL32(0, 0, 0, 80);

    dl->AddLine({ sx - r - 1, sy }, { sx - gap - 1, sy }, out, 1.5f);
    dl->AddLine({ sx + gap - 1, sy }, { sx + r - 1, sy }, out, 1.5f);
    dl->AddLine({ sx, sy - r - 1 }, { sx, sy - gap - 1 }, out, 1.5f);
    dl->AddLine({ sx, sy + gap - 1 }, { sx, sy + r - 1 }, out, 1.5f);

    dl->AddLine({ sx - r, sy }, { sx - gap, sy }, col, 1.5f);
    dl->AddLine({ sx + gap, sy }, { sx + r, sy }, col, 1.5f);
    dl->AddLine({ sx, sy - r }, { sx, sy - gap }, col, 1.5f);
    dl->AddLine({ sx, sy + gap }, { sx, sy + r }, col, 1.5f);

    dl->AddCircleFilled({ sx, sy }, 1.8f, col);
}

} // namespace SceneViewLayerRender
