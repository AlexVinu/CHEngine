#pragma once

namespace Sandbox { struct EditorContext; }

namespace SceneViewLayerPlay {

void EnterPlayMode(Sandbox::EditorContext& ctx);
void EnterPauseMode(Sandbox::EditorContext& ctx);
void ResumeFromPause(Sandbox::EditorContext& ctx);
void StopPlayMode(Sandbox::EditorContext& ctx);

} // namespace SceneViewLayerPlay
