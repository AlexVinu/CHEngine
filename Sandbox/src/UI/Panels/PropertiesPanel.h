#pragma once

#include <imgui.h>

namespace Sandbox {

struct EditorContext;

class PropertiesPanel
{
public:
    void Draw(EditorContext& ctx, ImVec2 pos, ImVec2 size, bool reset_layout);
};

} // namespace Sandbox
