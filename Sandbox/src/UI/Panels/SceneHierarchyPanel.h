#pragma once

#include "EditorUiHost.h"
#include <CHEngine/Render/RenderFacade.h>
#include <Render/Handles.h>

#include <imgui.h>

namespace Sandbox {

class SceneHierarchyPanel
{
public:
    void Draw(EditorUiHost& host, ImVec2 pos, ImVec2 size, bool reset_layout);
    // Draw only the content (no panel wrapper) — used as a tab inside CameraPanel
    void DrawContent(EditorUiHost& host);

private:
    CHEngine::TextureHandle m_Logo;
    float                   m_LogoAspect = 1.0f; // width / height
    bool                    m_LogoLoaded = false;

    void EnsureLogo();
};

} // namespace Sandbox
