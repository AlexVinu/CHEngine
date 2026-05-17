#include "SceneViewLayer_GizmoBar.h"

#include "SceneViewLayer.h"
#include "SceneViewLayerAccess.h"
#include "SceneViewLayerHost.h"
#include "SetTransformCommand.h"
#include "UIThemeActive.h"

#include <CHEngine/Application.h>
#include <Render/Handles.h>

#include <imgui.h>
#include <ImGuizmo.h>

namespace SceneViewLayerGizmoBar {

static const float kBtnSize    = 30.0f;   // размер кнопки (квадрат)
static const float kIconPad    = 4.0f;    // padding внутри ImageButton с каждой стороны
static const float kMarginX    = 10.0f;
static const float kMarginY    = 6.0f;

// ── Иконки (загружаются один раз) ────────────────────────────────────────────
static CHEngine::TextureHandle s_IconTranslate;
static CHEngine::TextureHandle s_IconRotate;
static CHEngine::TextureHandle s_IconScale;
static bool                   s_IconsLoaded = false;

static void EnsureIcons()
{
    if (s_IconsLoaded) return;
    s_IconTranslate = CHEngine::Application::Get().Render().CreateTextureFromFile("editor_assets/icons/icon_translate.png");
    s_IconRotate    = CHEngine::Application::Get().Render().CreateTextureFromFile("editor_assets/icons/icon_rotate.png");
    s_IconScale     = CHEngine::Application::Get().Render().CreateTextureFromFile("editor_assets/icons/icon_scale.png");
    s_IconsLoaded   = true; // даже если не загрузились — не пытаемся каждый кадр
}

static ImTextureID ToImTex(CHEngine::TextureHandle h)
{
    if (!h.IsValid()) return static_cast<ImTextureID>(0);
    auto* f = CHEngine::Application::Get().Render().GetRenderFactory();
    return f ? static_cast<ImTextureID>(f->GetTextureNativeID(h)) : static_cast<ImTextureID>(0);
}

void Draw(SceneViewLayer& layer)
{
    EnsureIcons();

    Ref<EditorWorldContext> ctx = SceneViewLayerAccess::ActiveRef(layer);
    if (ctx->GetSessionState() != SceneSession::State::Edit)
        return;

    // Position: right below toolbar, left edge of viewport
    const float toolbarH = UIActive::g_Layout.toolbarH;
    ImGui::SetNextWindowPos(ImVec2(kMarginX, toolbarH + kMarginY), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.88f);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(5.0f, 5.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,    ImVec2(0.0f, 3.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,  8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);

    const ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoTitleBar       |
        ImGuiWindowFlags_NoResize         |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoMove           |
        ImGuiWindowFlags_NoSavedSettings  |
        ImGuiWindowFlags_NoScrollbar      |
        ImGuiWindowFlags_NoNav            |
        ImGuiWindowFlags_NoFocusOnAppearing;

    ImGui::Begin("##gizmo_bar", nullptr, kFlags);
    ImGui::PopStyleVar(4);

    const ImGuizmo::OPERATION op = ctx->GizmoOperation;

    const ImVec4 kActive   = ImVec4(0.251f, 0.612f, 1.000f, 1.00f);
    const ImVec4 kNormal   = ImVec4(0.18f,  0.18f,  0.20f,  1.00f);
    const ImVec4 kHover    = ImVec4(0.25f,  0.25f,  0.28f,  1.00f);

    // Размер картинки внутри ImageButton:
    // ImageButton total = iconDraw + 2*kIconPad → должен равняться kBtnSize
    const float  kIconDraw = kBtnSize - 2.0f * kIconPad;
    const ImVec2 kIconSize = ImVec2(kIconDraw, kIconDraw);

    // Metal: UV без флипа; OGL: флип Y
    const bool   isMetal = (CHEngine::Application::Get().GetRenderAPIType() == CHEngine::ERenderAPI::METAL);
    const ImVec2 kUV0    = isMetal ? ImVec2(0,0) : ImVec2(0,1);
    const ImVec2 kUV1    = isMetal ? ImVec2(1,1) : ImVec2(1,0);

    // Рисует кнопку с иконкой (или текстом-fallback).
    // Явно задаём FramePadding = kIconPad чтобы ImageButton был ровно kBtnSize × kBtnSize
    auto gizmoBtn = [&](const char* strId, const char* tooltip,
                        CHEngine::TextureHandle iconTex, const char* fallback,
                        ImGuizmo::OPERATION thisOp) -> bool
    {
        const bool active = (op == thisOp);
        ImGui::PushStyleColor(ImGuiCol_Button,        active ? kActive : kNormal);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active ? kActive : kHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  kActive);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(kIconPad, kIconPad));

        ImTextureID texId = ToImTex(iconTex);
        bool clicked = false;
        if (texId)
        {
            ImVec4 tint = active ? ImVec4(1,1,1,1) : ImVec4(0.8f, 0.8f, 0.8f, 0.9f);
            clicked = ImGui::ImageButton(strId, texId, kIconSize,
                                         kUV0, kUV1, ImVec4(0,0,0,0), tint);
        }
        else
        {
            clicked = ImGui::Button(fallback, ImVec2(kBtnSize, kBtnSize));
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", tooltip);
        return clicked;
    };

    auto toggleBtn = [&](const char* label, const char* tooltip, bool active) -> bool
    {
        ImGui::PushStyleColor(ImGuiCol_Button,        active ? kActive : kNormal);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active ? kActive : kHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  kActive);
        bool clicked = ImGui::Button(label, ImVec2(kBtnSize, kBtnSize));
        ImGui::PopStyleColor(3);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", tooltip);
        return clicked;
    };

    Sandbox::SceneViewLayerHost host(layer);

    // Translate
    if (gizmoBtn("##gizmo_tra", "Translate  [T]", s_IconTranslate, "T", ImGuizmo::TRANSLATE))
        host.GetCommandStack().Push(MakeScope<Sandbox::CallbackCommand>(
            [&ctx] { ctx->GizmoOperation = ImGuizmo::TRANSLATE; }, [] {}, false));

    // Rotate
    if (gizmoBtn("##gizmo_rot", "Rotate  [R]", s_IconRotate, "R", ImGuizmo::ROTATE))
        host.GetCommandStack().Push(MakeScope<Sandbox::CallbackCommand>(
            [&ctx] { ctx->GizmoOperation = ImGuizmo::ROTATE; }, [] {}, false));

    // Scale
    if (gizmoBtn("##gizmo_scl", "Scale  [S]", s_IconScale, "S", ImGuizmo::SCALE))
        host.GetCommandStack().Push(MakeScope<Sandbox::CallbackCommand>(
            [&ctx] { ctx->GizmoOperation = ImGuizmo::SCALE; }, [] {}, false));

    // Separator
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.4f, 0.4f, 0.4f, 0.4f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // Local / World
    const bool local = (ctx->GizmoMode == ImGuizmo::LOCAL);
    if (toggleBtn(local ? "L" : "W",
                  local ? "Local space  (click → World)" : "World space  (click → Local)",
                  local))
        host.GetCommandStack().Push(MakeScope<Sandbox::CallbackCommand>(
            [&ctx, local] { ctx->GizmoMode = local ? ImGuizmo::WORLD : ImGuizmo::LOCAL; },
            [] {}, false));

    // Grid
    bool& showGrid = SceneViewLayerAccess::Viewport(layer).ShowGrid();
    if (toggleBtn("#", showGrid ? "Grid ON" : "Grid OFF", showGrid))
        showGrid = !showGrid;

    ImGui::End();
}

} // namespace SceneViewLayerGizmoBar
