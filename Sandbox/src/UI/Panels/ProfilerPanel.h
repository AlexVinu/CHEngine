#pragma once

namespace Sandbox {

struct EditorContext;

class ProfilerPanel
{
public:
    void Draw(EditorContext& ctx);

    // Tiling mode — always shows, uses window set by tiling system
    void DrawInPanel(EditorContext& ctx);
};

} // namespace Sandbox
