#pragma once

#include "SceneViewLayerHost.h"

namespace Sandbox {

class ProfilerPanel
{
public:
    void Draw(SceneViewLayerHost& host);

    // Tiling mode — always shows, uses window set by tiling system
    void DrawInPanel(SceneViewLayerHost& host);
};

} // namespace Sandbox
