#pragma once

#include "SceneViewLayerHost.h"
#include "SceneHierarchyPanel.h"

#include <imgui.h>

namespace Sandbox {

class CameraPanel
{
public:
    void Draw(SceneViewLayerHost& host, ImVec2 pos, ImVec2 size, bool reset_layout);

private:
    SceneHierarchyPanel m_Hierarchy;
};

} // namespace Sandbox
