#pragma once

#include "EditorUiHost.h"

#include <imgui.h>

namespace Sandbox {

class ToolbarPanel
{
public:
    void Draw(EditorUiHost& host, ImVec2 pos, ImVec2 size);
};

} // namespace Sandbox
