#pragma once

#include <CHEngine.h>

namespace Sandbox { struct EditorContext; }

namespace SceneViewLayerMousePicking {

/// Performs mouse picking on LMB click in the viewport.
/// Computes a ray from the camera through the clicked pixel and tests
/// against all entities with MeshComponent. Selects the closest hit.
void TryPick(Sandbox::EditorContext& ctx);

} // namespace SceneViewLayerMousePicking
