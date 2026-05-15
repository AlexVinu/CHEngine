#include "SceneViewLayer_ShiftAMenu.h"

#include "SceneViewLayer.h"
#include "SceneViewLayerAccess.h"
#include "SceneViewLayerHost.h"
#include "EditorPopupState.h"
#include "UIThemeRetroOS.h"

#include <imgui.h>

namespace SceneViewLayerShiftAMenu {

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
    Ref<EditorWorldContext> ctxRef = SceneViewLayerAccess::ActiveRef(layer);
    EditorWorldContext& ctx = *ctxRef;

    // Open on Shift+A (viewport hovered, edit mode)
    if (viewport.IsViewportHovered() && ctx.GetSessionState() == SceneSession::State::Edit)
    {
        ImGuiIO& io = ImGui::GetIO();
        if (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_A, false))
        {
            EditorPopup::Open(EditorPopup::ID::ShiftA); // closes any other popup
            s_Pos = ImGui::GetMousePos();
        }
    }

    if (!EditorPopup::IsOpen(EditorPopup::ID::ShiftA))
        return;

    // Escape always closes
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    { EditorPopup::Close(); return; }

    ImGui::SetNextWindowPos(s_Pos, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.97f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(10.0f, 8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 7.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,    ImVec2(8.0f, 3.0f));
    ImGui::Begin("##shift_a_menu", nullptr, kFlags);
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

    Sandbox::SceneViewLayerHost host(layer);
    const float w = 150.0f;

    // ── Mesh ──────────────────────────────────────────────────────────────────
    UIThemeRetro::PushHeadingFont();
    ImGui::TextDisabled("Mesh");
    UIThemeRetro::PopFont();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 4.0f));
    if (ImGui::Selectable("  Cube",   false, 0, ImVec2(w, 0))) { host.AddCubePrimitive();   EditorPopup::Close(); }
    if (ImGui::Selectable("  Sphere", false, 0, ImVec2(w, 0))) { host.AddSpherePrimitive(); EditorPopup::Close(); }
    ImGui::PopStyleVar();

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    // ── Light ─────────────────────────────────────────────────────────────────
    UIThemeRetro::PushHeadingFont();
    ImGui::TextDisabled("Light");
    UIThemeRetro::PopFont();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 4.0f));
    if (ImGui::Selectable("  Point",       false, 0, ImVec2(w, 0))) { host.AddPointLight();       EditorPopup::Close(); }
    if (ImGui::Selectable("  Directional", false, 0, ImVec2(w, 0))) { host.AddDirectionalLight(); EditorPopup::Close(); }
    if (ImGui::Selectable("  Spot",        false, 0, ImVec2(w, 0))) { host.AddSpotLight();        EditorPopup::Close(); }
    ImGui::PopStyleVar();

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    // ── Camera ────────────────────────────────────────────────────────────────
    if (ImGui::Selectable("  Camera", false, 0, ImVec2(w, 0))) { host.AddCameraEntity(); EditorPopup::Close(); }

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    // ── Empty ─────────────────────────────────────────────────────────────────
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 4.0f));
    if (ImGui::Selectable("  Empty Entity", false, 0, ImVec2(w, 0))) { host.AddEmptyEntity(); EditorPopup::Close(); }
    ImGui::PopStyleVar();

    ImGui::End();
}

} // namespace SceneViewLayerShiftAMenu
