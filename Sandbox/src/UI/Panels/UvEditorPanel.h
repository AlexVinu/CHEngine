#pragma once

#include "EditorUiHost.h"

#include <CHEngine.h>

#include <imgui.h>

namespace Sandbox {

class UvEditorPanel
{
public:
    void Draw(EditorUiHost& host);

private:
    int m_SelectedVertex = -1;
    int m_DraggedGroup = -1;
    bool m_Dragging = false;
};

} // namespace Sandbox
