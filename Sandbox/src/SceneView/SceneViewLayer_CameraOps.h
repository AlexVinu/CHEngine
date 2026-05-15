#pragma once

class SceneViewLayer;

namespace SceneViewLayerCameraOps {

void ApplyOrbit(SceneViewLayer& layer);
void SetViewPreset(SceneViewLayer& layer, float yaw_degrees, float pitch_degrees);
void FocusOnSelected(SceneViewLayer& layer);
void UpdateEditorCameraInput(SceneViewLayer& layer);
void PrepareEditorCameraFrame(SceneViewLayer& layer);

} // namespace SceneViewLayerCameraOps

namespace SceneViewLayerRender {

void DrawOrbitIndicator(SceneViewLayer& layer);

} // namespace SceneViewLayerRender
