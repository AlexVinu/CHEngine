#pragma once

class SceneViewLayer;

namespace SceneViewLayerPlay {

void EnterPlayMode(SceneViewLayer& layer);
void EnterPauseMode(SceneViewLayer& layer);
void ResumeFromPause(SceneViewLayer& layer);
void StopPlayMode(SceneViewLayer& layer);

} // namespace SceneViewLayerPlay
