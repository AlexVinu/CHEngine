#pragma once

namespace Sandbox { struct EditorContext; }

namespace SceneViewLayerCameraOps {

void ApplyOrbit(Sandbox::EditorContext& ctx);
void SetViewPreset(Sandbox::EditorContext& ctx, float yaw_degrees, float pitch_degrees);
void FocusOnSelected(Sandbox::EditorContext& ctx);
void ResetViewportCamera(Sandbox::EditorContext& ctx);
void UpdateEditorCameraInput(Sandbox::EditorContext& ctx);
void PrepareEditorCameraFrame(Sandbox::EditorContext& ctx);

} // namespace SceneViewLayerCameraOps

namespace SceneViewLayerRender {

void DrawOrbitIndicator(Sandbox::EditorContext& ctx);

} // namespace SceneViewLayerRender
