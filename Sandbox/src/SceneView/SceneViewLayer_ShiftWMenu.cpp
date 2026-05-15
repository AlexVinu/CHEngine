#include "SceneViewLayer_ShiftWMenu.h"

#include "SceneViewLayer.h"
#include "SceneViewLayerAccess.h"
#include "TilingManager.h"
#include "UIThemeRetroOS.h"

#include <imgui.h>

namespace SceneViewLayerShiftWMenu {

static bool   s_Open = false;
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

    // Open on Shift+W (anywhere in the editor, not just viewport)
    if (!ImGui::GetIO().WantTextInput)
    {
        ImGuiIO& io = ImGui::GetIO();
        if (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_W, false))
        {
            s_Open = !s_Open;   // toggle
            s_Pos  = ImGui::GetMousePos();
        }
    }

    if (!s_Open) return;

    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    { s_Open = false; return; }

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
    { s_Open = false; return; }

    ImGui::SetNextWindowPos(s_Pos, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.97f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(10.0f, 8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 7.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,    ImVec2(8.0f, 4.0f));
    ImGui::Begin("##shift_w_menu", nullptr, kFlags);
    ImGui::PopStyleVar(3);

    const float w = 170.0f;

    UIThemeRetro::PushHeadingFont();
    ImGui::TextDisabled("Add Window  [Shift+W]");
    UIThemeRetro::PopFont();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 5.0f));

    struct Entry { Sandbox::PanelID id; const char* label; };
    static const Entry kEntries[] = {
        { Sandbox::PanelID::Viewport,       "Viewport"       },
        { Sandbox::PanelID::Inspector,      "Inspector"      },
        { Sandbox::PanelID::Properties,     "Properties"     },
        { Sandbox::PanelID::ContentBrowser, "Content Browser"},
        { Sandbox::PanelID::None,           nullptr          }, // separator
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
            inLayout ? ImGuiSelectableFlags_Disabled : 0,
            ImVec2(w, 0)))
        {
            // Start ghost drag in TilingManager
            tiling.StartGhostPlacement(e.id);
            s_Open = false;
        }

        if (inLayout) ImGui::PopStyleColor();
    }

    ImGui::PopStyleVar();
    ImGui::End();
}

} // namespace SceneViewLayerShiftWMenu
