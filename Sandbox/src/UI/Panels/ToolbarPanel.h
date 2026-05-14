#pragma once

#include "SceneViewLayerHost.h"

#include <imgui.h>

namespace Sandbox {

class ToolbarPanel
{
public:
    void Draw(SceneViewLayerHost& host, ImVec2 pos, ImVec2 size);
};

} // namespace Sandbox
