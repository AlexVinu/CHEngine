#pragma once

#include <imgui.h>

#include <string>

namespace Sandbox {

struct EditorContext;

/// Standalone window listing all `*.chscene` files in <project>/Scenes/.
/// Lets the user open, delete, rename, set-as-startup, and create scenes.
class SceneBrowserPanel
{
public:
    void OnImGuiRender(EditorContext& ctx);

    bool IsOpen() const { return m_Open; }
    void SetOpen(bool open) { m_Open = open; }
    void Toggle() { m_Open = !m_Open; }

private:
    bool m_Open = false;

    // Inline-rename state.
    char m_RenameBuf[128] = {};
    std::string m_RenameTarget; // rel path being renamed, empty = none

    // Delete-confirm popup state.
    std::string m_PendingDelete; // rel path to delete after confirm
};

} // namespace Sandbox
