#include "SceneViewLayer_CameraOps.h"

#include "EditorContext.h"
#include "EditorPopupState.h"

#include <CHEngine/Scene/Components.h>
#include <CHEngine/Scene/Entity.h>

#include <CHEngine/Application.h>

#include <glm/gtc/constants.hpp>
#include <imgui.h>
#include <ImGuizmo.h>

#include <algorithm>

namespace SceneViewLayerCameraOps {

void ApplyOrbit(Sandbox::EditorContext& ctx)
{
    EditorWorldContext* s = ctx.ActiveCtx();
    ctx.CameraController.ApplyOrbit(s->ViewportCamera.get(), s->EditorCameraState);
}

void SetViewPreset(Sandbox::EditorContext& ctx, float yaw_degrees, float pitch_degrees)
{
    EditorWorldContext* s = ctx.ActiveCtx();
    ctx.CameraController.SetViewPreset(
        yaw_degrees, pitch_degrees, s->ViewportCamera.get(), s->EditorCameraState);
}

void FocusOnSelected(Sandbox::EditorContext& ctx)
{
    constexpr float k_DefaultFocusRadius = 0.5f;

    EditorWorldContext* s = ctx.ActiveCtx();
    auto active_scene = s->EditorScene;
    CHEngine::Entity* entity = active_scene ? active_scene->TryGetEntity(s->SelectedEntity) : nullptr;
    if (!entity
        || !entity->HasComponent<CHEngine::TransformComponent>()
        || !entity->HasComponent<CHEngine::MeshComponent>())
        return;

    const auto& transform = entity->GetComponent<CHEngine::TransformComponent>().ObjectTransform;
    const auto& meshRefFocus = entity->GetComponent<CHEngine::MeshComponent>().Mesh;

    CHEngine::MeshLoader* meshLoader = CHEngine::Application::Get().Resources().GetMeshLoader();
    float max_radius = k_DefaultFocusRadius;
    if (meshRefFocus.IsValid())
    {
        if (const auto* rec = meshLoader->GetGpuRecord(meshRefFocus.Handle()))
        {
            for (const auto& vertex : rec->vertices)
            {
                const float distance = glm::length(vertex.Position);
                if (distance > max_radius)
                    max_radius = distance;
            }
        }
    }

    const float scale_max = std::max({ transform.Scale.x, transform.Scale.y, transform.Scale.z });
    ctx.CameraController.FocusOnPoint(
        transform.Position, max_radius * scale_max, s->ViewportCamera.get(), s->EditorCameraState);
}

void ResetViewportCamera(Sandbox::EditorContext& ctx)
{
    EditorWorldContext* s = ctx.ActiveCtx();
    CHEngine::EditorCamera* viewportCamera = s->ViewportCamera.get();
    Sandbox::EditorCameraState& camera_state = s->EditorCameraState;
    camera_state.OrbitTarget = { 0.0f, 0.0f, 0.0f };
    camera_state.OrbitDist = 8.0f;
    viewportCamera->SetYaw(glm::radians(-90.0f));
    viewportCamera->SetPitch(glm::radians(-15.0f));
    ApplyOrbit(ctx);
}

void UpdateEditorCameraInput(Sandbox::EditorContext& ctx)
{
    CHEngine::InputSystem* input_system = CHEngine::Application::Get().InputSystem();

    Sandbox::EditorCameraController::InputSnapshot inputSnapshot{};
    inputSnapshot.IsViewportHovered = ctx.Viewport.IsViewportHovered()
        && !EditorPopup::AnyOpen();
    inputSnapshot.IsGizmoUsing = ImGuizmo::IsUsing();
    inputSnapshot.IsCtrlPressed = input_system->IsModifierDown(CHEngine::Mod_Ctrl);
    inputSnapshot.IsFocusPressed = input_system->Triggered("Editor.Camera.Focus");
    inputSnapshot.MouseWheel = input_system->GetAxis(CHEngine::InputSystem::Axis::MouseWheel);
    inputSnapshot.MouseDelta = { input_system->GetAxis(CHEngine::InputSystem::Axis::MouseDeltaX), input_system->GetAxis(CHEngine::InputSystem::Axis::MouseDeltaY) };

    inputSnapshot.IsOrbitByRmbDrag       = input_system->Down("Editor.Camera.OrbitRmb");
    inputSnapshot.IsOrbitByAltLmbDrag    = input_system->Down("Editor.Camera.OrbitAltLmb");
    inputSnapshot.IsOrbitByMmbDrag       = input_system->Down("Editor.Camera.OrbitMmb");

    inputSnapshot.IsPanByAltShiftLmbDrag = input_system->Down("Editor.Camera.PanAltShift");
    inputSnapshot.IsPanByShiftMmbDrag    = input_system->Down("Editor.Camera.PanShiftMmb");
    inputSnapshot.IsPanByShiftRmbDrag    = input_system->Down("Editor.Camera.PanShiftRmb");

    EditorWorldContext* s = ctx.ActiveCtx();
    ctx.CameraController.UpdateCameraInput(
        inputSnapshot, s->ViewportCamera.get(), s->EditorCameraState, s->EditorScene, s->SelectedEntity);

    if (inputSnapshot.IsFocusPressed && s->SelectedEntity.IsValid())
        FocusOnSelected(ctx);

    ctx.CameraController.OnUpdate(
        s->ViewportCamera.get(), s->EditorCameraState, s->EditorScene, s->SelectedEntity, inputSnapshot);
}

void PrepareEditorCameraFrame(Sandbox::EditorContext& ctx)
{
    EditorWorldContext* s = ctx.ActiveCtx();
    // In Play the world uses scene cameras only; editor orbit camera must not move or consume input.
    if (s->GetSessionState() == SceneSession::State::Play)
        return;

    UpdateEditorCameraInput(ctx);
}

} // namespace SceneViewLayerCameraOps

namespace SceneViewLayerRender {

void DrawOrbitIndicator(Sandbox::EditorContext& ctx)
{
    EditorWorldContext* s = ctx.ActiveCtx();
    if (s->GetSessionState() == SceneSession::State::Play || s->GetSessionState() == SceneSession::State::Pause)
        return;

    ImGuiIO& io = ImGui::GetIO();
    const float W = io.DisplaySize.x;
    const float H = io.DisplaySize.y;

    auto* viewport_camera = s->ViewportCamera.get();
    if (!viewport_camera)
        return;
    glm::mat4 view = viewport_camera->GetViewMatrix();
    glm::mat4 proj = viewport_camera->GetProjectionMatrix();

    glm::vec4 clip = proj * view * glm::vec4(s->EditorCameraState.OrbitTarget, 1.0f);
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
