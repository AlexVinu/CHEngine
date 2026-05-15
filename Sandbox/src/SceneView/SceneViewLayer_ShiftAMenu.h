#pragma once

class SceneViewLayer;

namespace SceneViewLayerShiftAMenu {

// Call once per ImGui frame while the viewport is active.
// Opens a Blender-style "Add" popup on Shift+A and dispatches the chosen primitive.
void Draw(SceneViewLayer& layer);

} // namespace SceneViewLayerShiftAMenu
