#pragma once

#include "SceneViewLayerHost.h"

#include <CHEngine/Render/RenderFacade.h>
#include <Render/Handles.h>
#include <imgui.h>

namespace Sandbox {

class ToolbarPanel
{
public:
    ToolbarPanel();

    void Draw(SceneViewLayerHost& host, ImVec2 pos, ImVec2 size);

private:
    void LoadIcons();

    CHEngine::TextureHandle m_IconTranslate;
    CHEngine::TextureHandle m_IconRotate;
    CHEngine::TextureHandle m_IconScale;
    CHEngine::TextureHandle m_IconPlay;
    CHEngine::TextureHandle m_IconPause;
    CHEngine::TextureHandle m_IconStop;
    CHEngine::TextureHandle m_IconResume;
    bool m_IconsLoaded = false;
};

} // namespace Sandbox
