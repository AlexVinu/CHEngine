#pragma once

#include <CHEngine/Application.h>
#include <Render/Handles.h>
#include <imgui.h>

namespace Sandbox {

struct EditorContext;

class SceneHierarchyPanel
{
public:
    void Draw(EditorContext& ctx, ImVec2 pos, ImVec2 size, bool reset_layout);
    void DrawContent(EditorContext& ctx);

private:
    CHEngine::TextureHandle m_Logo;
    float                   m_LogoAspect = 1.0f;
    bool                    m_LogoLoaded = false;

    void EnsureLogo();
};

} // namespace Sandbox
