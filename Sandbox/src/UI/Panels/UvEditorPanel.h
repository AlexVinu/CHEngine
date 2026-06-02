#pragma once

#include <CHEngine.h>

#include <imgui.h>
#include <vector>

namespace Sandbox {

struct EditorContext;

class UvEditorPanel
{
public:
    void Draw(EditorContext& ctx);

private:
    int m_SelectedVertex = -1;
    int m_DraggedGroup = -1;
    bool m_Dragging = false;

    CHEngine::EntityHandle m_CachedEntity;
    std::vector<std::vector<int>> m_CachedUVGroups;
    bool m_UVGroupsDirty = true;
};

} // namespace Sandbox
