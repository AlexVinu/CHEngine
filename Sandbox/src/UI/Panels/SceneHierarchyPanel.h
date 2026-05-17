#pragma once

#include "SceneViewLayerHost.h"
#include <CHEngine/Application.h>
#include <Render/Handles.h>
#include <imgui.h>

namespace Sandbox {

class SceneHierarchyPanel
{
public:
    void Draw(SceneViewLayerHost& host, ImVec2 pos, ImVec2 size, bool reset_layout);
    void DrawContent(SceneViewLayerHost& host);

private:
    CHEngine::TextureHandle m_Logo;
    float                   m_LogoAspect = 1.0f;
    bool                    m_LogoLoaded = false;

    void EnsureLogo();
};

} // namespace Sandbox
