#include "SceneViewLayer_ShiftAMenu.h"

#include "SceneViewLayer.h"
#include "SceneViewLayerAccess.h"
#include "SceneViewLayerHost.h"
#include "UIThemeRetroOS.h"

#include <imgui.h>

namespace SceneViewLayerShiftAMenu {

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
    Sandbox::EditorViewport& viewport = SceneViewLayerAccess::Viewport(layer);
    EditorWorldContext& ctx = SceneViewLayerAccess::Active(layer);

    if (viewport.IsViewportHovered() && ctx.SessionState == SceneSession::State::Edit)
    {
        ImGuiIO& io = ImGui::GetIO();
        if (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_A, false))
        {
            s_Open = true;
            s_Pos  = ImGui::GetMousePos();
        }
    }

    if (!s_Open)
        return;

    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))       { s_Open = false; return; }
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)) { s_Open = false; return; }

    ImGui::SetNextWindowPos(s_Pos, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.97f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(10.0f, 8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 7.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,    ImVec2(8.0f, 3.0f));
    ImGui::Begin("##shift_a_menu", nullptr, kFlags);
    ImGui::PopStyleVar(3);

    SceneViewLayerHost host(layer);
    const float w = 150.0f;

    // ── Empty ─────────────────────────────────────────────────────────────────
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 4.0f));
    if (ImGui::Selectable("  Empty Entity", false, 0, ImVec2(w, 0))) { host.AddEmptyEntity(); s_Open = false; }
    ImGui::PopStyleVar();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Mesh ──────────────────────────────────────────────────────────────────
    UIThemeRetro::PushHeadingFont();
    ImGui::TextDisabled("Mesh");
    UIThemeRetro::PopFont();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 4.0f));
    if (ImGui::Selectable("  Cube",   false, 0, ImVec2(w, 0))) { host.AddCubePrimitive();   s_Open = false; }
    if (ImGui::Selectable("  Sphere", false, 0, ImVec2(w, 0))) { host.AddSpherePrimitive(); s_Open = false; }
    ImGui::PopStyleVar();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Light ─────────────────────────────────────────────────────────────────
    UIThemeRetro::PushHeadingFont();
    ImGui::TextDisabled("Light");
    UIThemeRetro::PopFont();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 4.0f));
    if (ImGui::Selectable("  Point",       false, 0, ImVec2(w, 0))) { host.AddPointLight();       s_Open = false; }
    if (ImGui::Selectable("  Directional", false, 0, ImVec2(w, 0))) { host.AddDirectionalLight(); s_Open = false; }
    if (ImGui::Selectable("  Spot",        false, 0, ImVec2(w, 0))) { host.AddSpotLight();        s_Open = false; }
    ImGui::PopStyleVar();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Camera ────────────────────────────────────────────────────────────────
    if (ImGui::Selectable("  Camera", false, 0, ImVec2(w, 0))) { host.AddCameraEntity(); s_Open = false; }

    ImGui::End();
}

} // namespace SceneViewLayerShiftAMenu
