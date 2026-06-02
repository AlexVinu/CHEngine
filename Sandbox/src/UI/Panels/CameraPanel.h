#pragma once

#include "SceneHierarchyPanel.h"

#include <imgui.h>

namespace Sandbox {

struct EditorContext;

class CameraPanel
{
public:
    void Draw(EditorContext& ctx, ImVec2 pos, ImVec2 size, bool reset_layout);

private:
    SceneHierarchyPanel m_Hierarchy;
};

} // namespace Sandbox
