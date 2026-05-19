#include "SceneViewLayer_ShiftWMenu.h"

#include "SceneViewLayer.h"
#include "SceneViewLayerAccess.h"
#include "EditorPopupState.h"
#include <CHEngine/Application.h>
#include "TilingManager.h"
#include "UIThemeRetroOS.h"

#include <imgui.h>

namespace SceneViewLayerShiftWMenu {

static ImVec2 s_Pos;

static const ImGuiWindowFlags kFlags =
    ImGuiWindowFlags_NoTitleBar        |
    ImGuiWindowFlags_NoResize          |
    ImGuiWindowFlags_AlwaysAutoResize  |
    ImGuiWindowFlags_NoMove            |
    ImGuiWindowFlags_NoSavedSettings   |
    ImGuiWindowFlags_NoScrollbar       |
    ImGuiWindowFlags_NoNav;

void Draw(SceneViewLayer& layer)
{
    Sandbox::TilingManager& tiling = SceneViewLayerAccess::Tiling(layer);

    // Open on Shift+W
    if (!ImGui::GetIO().WantTextInput)
    {
        if (CHEngine::Application::Get().InputSystem()->Triggered("Editor.Menu.AddWindow"))
        {
            if (EditorPopup::IsOpen(EditorPopup::ID::ShiftW))
                EditorPopup::Close();          // toggle off
            else
            {
                EditorPopup::Open(EditorPopup::ID::ShiftW); // closes any other popup
                s_Pos = ImGui::GetMousePos();
            }
        }
    }

    if (!EditorPopup::IsOpen(EditorPopup::ID::ShiftW))
        return;

    if (CHEngine::Application::Get().InputSystem()->Triggered("Editor.Popup.Close"))
    { EditorPopup::Close(); return; }

    ImGui::SetNextWindowPos(s_Pos, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.97f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(10.0f, 8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 7.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,    ImVec2(8.0f, 4.0f));
    ImGui::Begin("##shift_w_menu", nullptr, kFlags);
    ImGui::PopStyleVar(3);

    // Click outside THIS popup window → close
    {
        ImVec2 wPos  = ImGui::GetWindowPos();
        ImVec2 wSize = ImGui::GetWindowSize();
        ImGuiIO& io  = ImGui::GetIO();
        bool outside = io.MousePos.x < wPos.x || io.MousePos.x > wPos.x + wSize.x ||
                       io.MousePos.y < wPos.y || io.MousePos.y > wPos.y + wSize.y;
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && outside)
        { EditorPopup::Close(); ImGui::End(); return; }
    }

    const float w = 170.0f;

    UIThemeRetro::PushHeadingFont();
    ImGui::TextDisabled("Add Window  [Shift+W]");
    UIThemeRetro::PopFont();

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 5.0f));

    struct Entry { Sandbox::PanelID id; const char* label; };
    static const Entry kEntries[] = {
        { Sandbox::PanelID::Viewport,       "Viewport"       },
        { Sandbox::PanelID::Inspector,      "Inspector"      },
        { Sandbox::PanelID::Properties,     "Properties"     },
        { Sandbox::PanelID::ContentBrowser, "Content Browser"},
        { Sandbox::PanelID::None,           nullptr          },
        { Sandbox::PanelID::Profiler,       "Profiler"       },
        { Sandbox::PanelID::UVEditor,       "UV Editor"      },
        { Sandbox::PanelID::SceneBrowser,   "Scene Browser"  },
        { Sandbox::PanelID::ScriptEditor,   "Script Editor"  },
    };

    for (const auto& e : kEntries)
    {
        if (!e.label) { ImGui::Separator(); continue; }

        bool inLayout = tiling.IsVisible(e.id);

        if (inLayout)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));

        char buf[80];
        snprintf(buf, sizeof(buf), "  %s%s", e.label, inLayout ? "  (open)" : "");

        if (ImGui::Selectable(buf, false,
            inLayout ? ImGuiSelectableFlags_Disabled : 0, ImVec2(w, 0)))
        {
            tiling.StartGhostPlacement(e.id);
            EditorPopup::Close();
        }

        if (inLayout) ImGui::PopStyleColor();
    }

    ImGui::PopStyleVar();
    ImGui::End();
}

} // namespace SceneViewLayerShiftWMenu
