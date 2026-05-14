#include "SceneViewLayer_ShiftAMenu.h"

#include "SceneViewLayer.h"
#include "SceneViewLayerAccess.h"
#include "SceneViewLayerHost.h"

#include <imgui.h>
#include <imgui_internal.h>

namespace SceneViewLayerShiftAMenu {

static bool    s_Open = false;
static ImVec2  s_Pos;

void Draw(SceneViewLayer& layer)
{
    Sandbox::EditorViewport& viewport = SceneViewLayerAccess::Viewport(layer);
    EditorWorldContext& ctx = SceneViewLayerAccess::Active(layer);

    // Detect Shift+A while viewport is hovered in Edit mode
    if (viewport.IsViewportHovered() && ctx.SessionState == SceneSession::State::Edit)
    {
        ImGuiIO& io = ImGui::GetIO();
        if (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_A, /*repeat=*/false))
        {
            s_Open = true;
            s_Pos  = ImGui::GetMousePos();
        }
    }

    if (!s_Open)
        return;

    // Close on Escape
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, /*repeat=*/false))
    {
        s_Open = false;
        return;
    }

    ImGui::SetNextWindowPos(s_Pos, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.96f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(8.0f, 6.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,    ImVec2(8.0f, 5.0f));

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar         |
        ImGuiWindowFlags_NoResize           |
        ImGuiWindowFlags_AlwaysAutoResize   |
        ImGuiWindowFlags_NoMove             |
        ImGuiWindowFlags_NoSavedSettings    |
        ImGuiWindowFlags_NoScrollbar        |
        ImGuiWindowFlags_NoNav;

    ImGui::Begin("##shift_a_menu", nullptr, flags);
    ImGui::PopStyleVar(3);

    // Close when user clicks outside
    if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        s_Open = false;
        ImGui::End();
        return;
    }

    ImGui::TextDisabled("Add");
    ImGui::Separator();

    SceneViewLayerHost host(layer);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 4.0f));

    ImGui::TextDisabled("  Mesh");
    if (ImGui::Selectable("    Cube",   false, 0, ImVec2(130, 0))) { host.AddCubePrimitive();   s_Open = false; }
    if (ImGui::Selectable("    Sphere", false, 0, ImVec2(130, 0))) { host.AddSpherePrimitive(); s_Open = false; }

    ImGui::PopStyleVar();
    ImGui::End();
}

} // namespace SceneViewLayerShiftAMenu
