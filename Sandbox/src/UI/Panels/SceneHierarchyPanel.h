#pragma once

#include "SceneViewLayerHost.h"

#include <imgui.h>

namespace Sandbox {

class SceneHierarchyPanel
{
public:
    void Draw(SceneViewLayerHost& host, ImVec2 pos, ImVec2 size, bool reset_layout);
};

} // namespace Sandbox
