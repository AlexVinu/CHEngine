#pragma once

#include "EditorUiHost.h"

#include <CHEngine/Render/RenderFacade.h>
#include <Render/Handles.h>
#include <imgui.h>

namespace Sandbox {

class ToolbarPanel
{
public:
    ToolbarPanel();

    void Draw(EditorUiHost& host, ImVec2 pos, ImVec2 size);

private:
    // Загружаем иконки один раз при создании панели
    void LoadIcons();

    CHEngine::TextureHandle m_IconTranslate;
    CHEngine::TextureHandle m_IconRotate;
    CHEngine::TextureHandle m_IconScale;
    bool m_IconsLoaded = false;
};

} // namespace Sandbox
