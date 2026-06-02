#pragma once

namespace Sandbox { struct EditorContext; }

namespace SceneViewLayerShiftAMenu {

// Call once per ImGui frame while the viewport is active.
// Opens a Blender-style "Add" popup on Shift+A and dispatches the chosen primitive.
void Draw(Sandbox::EditorContext& ctx);

} // namespace SceneViewLayerShiftAMenu
