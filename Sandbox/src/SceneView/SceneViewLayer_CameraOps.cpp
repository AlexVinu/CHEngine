#include "SceneViewLayer_CameraOps.h"

#include "SceneViewLayer.h"
#include "SceneViewLayerAccess.h"

#include <CHEngine/Input/Input.h>
#include <Input/KeyCodes.h>

#include <imgui.h>
#include <ImGuizmo.h>

namespace SceneViewLayerCameraOps {

void ApplyOrbit(SceneViewLayer& layer)
{
    EditorWorldContext& ctx = SceneViewLayerAccess::Active(layer);
    SceneViewLayerAccess::Camera(layer).ApplyOrbit(ctx.ViewportCamera.get());
}

void SetViewPreset(SceneViewLayer& layer, float yaw_degrees, float pitch_degrees)
{
    EditorWorldContext& ctx = SceneViewLayerAccess::Active(layer);
    SceneViewLayerAccess::Camera(layer).SetViewPreset(yaw_degrees, pitch_degrees, ctx.ViewportCamera.get());
}

void FocusOnSelected(SceneViewLayer& layer)
{
    EditorWorldContext& ctx = SceneViewLayerAccess::Active(layer);
    SceneViewLayerAccess::Camera(layer).FocusOnSelected(
        ctx.ViewportCamera.get(), ctx.ActiveScene.get(), ctx.SelectedEntity);
}

void UpdateEditorCameraInput(SceneViewLayer& layer)
{
    ImGuiIO& io = ImGui::GetIO();

    Sandbox::EditorCamera::InputSnapshot inputSnapshot{};
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

    EditorWorldContext& ctx = SceneViewLayerAccess::Active(layer);
    SceneViewLayerAccess::Camera(layer).UpdateCameraInput(
        inputSnapshot, ctx.ViewportCamera.get(), ctx.ActiveScene.get(), ctx.SelectedEntity);
}

void PrepareEditorCameraFrame(SceneViewLayer& layer)
{
    UpdateEditorCameraInput(layer);
    EditorWorldContext& ctx = SceneViewLayerAccess::Active(layer);
    SceneViewLayerAccess::Camera(layer).OnUpdate(ctx.ViewportCamera.get(),
                                                 ctx.ActiveScene.get(),
                                                 ctx.SelectedEntity,
                                                 SceneViewLayerAccess::Viewport(layer).IsViewportHovered(),
                                                 ImGuizmo::IsUsing());
}

} // namespace SceneViewLayerCameraOps
