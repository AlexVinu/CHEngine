#pragma once

#include "EditorUiHost.h"

#include <imgui.h>

namespace Sandbox {

class CameraPanel
{
public:
    void Draw(EditorUiHost& host, ImVec2 pos, ImVec2 size, bool reset_layout);
};

} // namespace Sandbox
