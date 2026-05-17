#include "chepch.h"
#include "UIRenderSystem.h"

#include "CHEngine/World/World.h"
#include "CHEngine/Scene/Scene.h"
#include "CHEngine/Scene/Components.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

namespace CHEngine {

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static ImU32 ToImCol(const glm::vec4& c, float alpha = 1.0f)
{
    return IM_COL32(
        (int)(c.r * 255),
        (int)(c.g * 255),
        (int)(c.b * 255),
        (int)(c.a * alpha * 255));
}

// ─────────────────────────────────────────────────────────────────────────────
UIRenderSystem::UIRect UIRenderSystem::ResolveRect(
    const UIRectTransformComponent& rt, float screenW, float screenH) const
{
    // Resolve anchor point in screen pixels
    float anchorX = rt.AnchorMin.x * screenW;
    float anchorY = rt.AnchorMin.y * screenH;

    // Offset from anchor, adjusted for pivot
    float pivotOffX = rt.Pivot.x * rt.Size.x;
    float pivotOffY = rt.Pivot.y * rt.Size.y;

    float x = anchorX + rt.Position.x - pivotOffX;
    float y = anchorY + rt.Position.y - pivotOffY;

    return { x, y, rt.Size.x, rt.Size.y };
}

bool UIRenderSystem::IsHovered(const UIRect& r) const
{
    ImVec2 mp = ImGui::GetIO().MousePos;
    return (mp.x >= r.x && mp.x <= r.x + r.w &&
            mp.y >= r.y && mp.y <= r.y + r.h);
}

ImFont* UIRenderSystem::GetFont(const std::string& path, float size)
{
    if (path.empty()) return ImGui::GetDefaultFont();

    std::string key = path + ":" + std::to_string((int)size);
    auto it = m_FontCache.find(key);
    if (it != m_FontCache.end()) return it->second;

    // Load font into ImGui atlas — note: atlas must be rebuilt after adding fonts.
    // In CHEngine the backend rebuilds automatically when TexReady = false.
    ImGuiIO& io = ImGui::GetIO();
    ImFont* font = io.Fonts->AddFontFromFileTTF(path.c_str(), size);
    if (!font) font = ImGui::GetDefaultFont();

    io.Fonts->Build();
    m_FontCache[key] = font;
    return font;
}

// ─────────────────────────────────────────────────────────────────────────────
// Draw helpers
// ─────────────────────────────────────────────────────────────────────────────

void UIRenderSystem::DrawPanel(ImDrawList* dl, const UIRect& r,
                                const UIPanelComponent& c, float alpha)
{
    ImVec2 tl(r.x, r.y), br(r.x + r.w, r.y + r.h);
    dl->AddRectFilled(tl, br, ToImCol(c.Color, alpha), c.CornerRadius);
    if (c.BorderWidth > 0.0f)
        dl->AddRect(tl, br, ToImCol(c.BorderColor, alpha), c.CornerRadius,
                    ImDrawFlags_None, c.BorderWidth);
}

void UIRenderSystem::DrawImage(ImDrawList* dl, const UIRect& r,
                                const UIImageComponent& c, float alpha)
{
    ImVec2 tl(r.x, r.y), br(r.x + r.w, r.y + r.h);
    // Solid colour (no texture or texture not loaded)
    dl->AddRectFilled(tl, br, ToImCol(c.Color, alpha));
    // TODO Phase 2: resolve TexturePath → ImTextureID and draw AddImage
}

void UIRenderSystem::DrawText(ImDrawList* dl, const UIRect& r,
                               const UITextComponent& c, float alpha)
{
    if (c.Text.empty()) return;

    ImFont* font = GetFont(c.FontPath, c.FontSize);
    ImU32   col  = ToImCol(c.Color, alpha);

    // Calculate text size for alignment
    ImVec2 textSz = font->CalcTextSizeA(c.FontSize, FLT_MAX,
        c.WordWrap ? r.w : -1.0f, c.Text.c_str());

    float tx = r.x, ty = r.y;

    switch (c.HorizontalAlign) {
    case UITextComponent::HAlign::Center: tx = r.x + (r.w - textSz.x) * 0.5f; break;
    case UITextComponent::HAlign::Right:  tx = r.x +  r.w - textSz.x;          break;
    default: break; // Left
    }
    switch (c.VerticalAlign) {
    case UITextComponent::VAlign::Middle: ty = r.y + (r.h - textSz.y) * 0.5f; break;
    case UITextComponent::VAlign::Bottom: ty = r.y +  r.h - textSz.y;          break;
    default: break; // Top
    }

    ImVec2 clipMin(r.x, r.y), clipMax(r.x + r.w, r.y + r.h);
    dl->PushClipRect(clipMin, clipMax);
    dl->AddText(font, c.FontSize, ImVec2(tx, ty), col,
                c.Text.c_str(), nullptr,
                c.WordWrap ? r.w : 0.0f);
    dl->PopClipRect();
}

void UIRenderSystem::DrawButton(ImDrawList* dl, const UIRect& r,
                                 const UIButtonComponent& c, float alpha,
                                 bool hovered, bool pressed)
{
    glm::vec4 col = c.NormalColor;
    if (!c.Interactable) col = c.DisabledColor;
    else if (pressed)    col = c.PressedColor;
    else if (hovered)    col = c.HoverColor;

    ImVec2 tl(r.x, r.y), br(r.x + r.w, r.y + r.h);
    dl->AddRectFilled(tl, br, ToImCol(col, alpha), c.CornerRadius);
}

void UIRenderSystem::DrawSlider(ImDrawList* dl, const UIRect& r,
                                 const UISliderComponent& c, float alpha)
{
    // Background track
    ImVec2 tl(r.x, r.y + r.h * 0.35f);
    ImVec2 br(r.x + r.w, r.y + r.h * 0.65f);
    dl->AddRectFilled(tl, br, ToImCol(c.BackgroundColor, alpha), 4.0f);

    // Fill
    float norm  = (c.Max > c.Min) ? (c.Value - c.Min) / (c.Max - c.Min) : 0.0f;
    float fillW = r.w * std::clamp(norm, 0.0f, 1.0f);
    if (fillW > 0.0f)
        dl->AddRectFilled(tl, ImVec2(tl.x + fillW, br.y),
                          ToImCol(c.FillColor, alpha), 4.0f);

    // Handle
    float hx = r.x + fillW;
    float hy = r.y + r.h * 0.5f;
    float hs = c.HandleSize * 0.5f;
    dl->AddCircleFilled(ImVec2(hx, hy), hs, ToImCol(c.HandleColor, alpha));
    dl->AddCircle(ImVec2(hx, hy), hs, IM_COL32(0,0,0,60));
}

// ─────────────────────────────────────────────────────────────────────────────
// Main render loop
// ─────────────────────────────────────────────────────────────────────────────

void UIRenderSystem::Run(World& world, DeferredOps& /*deferred*/, Timestep /*ts*/)
{
    auto scene = world.GetSceneRef();
    if (!scene) return;

    // Must be inside an ImGui frame
    if (!ImGui::GetCurrentContext()) return;

    ImGuiIO& io  = ImGui::GetIO();
    float    sw  = io.DisplaySize.x;
    float    sh  = io.DisplaySize.y;
    if (sw <= 0 || sh <= 0) return;

    // ── Collect all screen-space canvas entities ───────────────────────────
    // We only handle ScreenSpaceOverlay in Phase 1.
    // World-space canvases are Phase 5.
    struct DrawEntry {
        int       sortOrder;
        int       zOrder;
        EntityHandle handle;
    };
    std::vector<DrawEntry> entries;

    scene->ForEach<UIRectTransformComponent>(
        [&](EntityHandle h, const UUID&, UIRectTransformComponent& rt)
        {
            // Check if entity (or any ancestor) has a ScreenSpaceOverlay canvas.
            // Phase 1 simplification: any entity with UIRectTransform is drawn.
            int sortOrder = 0;
            auto* e = scene->TryGetEntity(h);
            if (e && e->HasComponent<UICanvasComponent>()) {
                if (e->GetComponent<UICanvasComponent>().Mode ==
                    UICanvasComponent::RenderMode::WorldSpace) return;
                sortOrder = e->GetComponent<UICanvasComponent>().SortOrder;
            }
            entries.push_back({ sortOrder, rt.ZOrder, h });
        });

    // Sort by canvas sort order, then z-order within canvas
    std::sort(entries.begin(), entries.end(), [](const DrawEntry& a, const DrawEntry& b) {
        if (a.sortOrder != b.sortOrder) return a.sortOrder < b.sortOrder;
        return a.zOrder < b.zOrder;
    });

    // Use foreground draw list so UI is always on top
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    for (auto& entry : entries)
    {
        auto* e = scene->TryGetEntity(entry.handle);
        if (!e) continue;

        auto& rt    = e->GetComponent<UIRectTransformComponent>();
        UIRect rect = ResolveRect(rt, sw, sh);
        float  alpha = rt.Alpha;

        // ── Panel ─────────────────────────────────────────────────────────
        if (e->HasComponent<UIPanelComponent>())
            DrawPanel(dl, rect, e->GetComponent<UIPanelComponent>(), alpha);

        // ── Image ─────────────────────────────────────────────────────────
        if (e->HasComponent<UIImageComponent>())
            DrawImage(dl, rect, e->GetComponent<UIImageComponent>(), alpha);

        // ── Button ────────────────────────────────────────────────────────
        if (e->HasComponent<UIButtonComponent>()) {
            auto& btn = e->GetComponent<UIButtonComponent>();
            bool hov  = btn.Interactable && IsHovered(rect);
            bool press= hov && ImGui::IsMouseDown(ImGuiMouseButton_Left);
            bool click= hov && ImGui::IsMouseReleased(ImGuiMouseButton_Left);
            DrawButton(dl, rect, btn, alpha, hov, press);

            // Fire OnClick via Lua (handled by LuaScriptSystem on next frame)
            // We write a flag into a thread-safe queue — Phase 3 TODO.
            (void)click;
        }

        // ── Slider ────────────────────────────────────────────────────────
        if (e->HasComponent<UISliderComponent>())
            DrawSlider(dl, rect, e->GetComponent<UISliderComponent>(), alpha);

        // ── Text (drawn last so it's on top of background elements) ───────
        if (e->HasComponent<UITextComponent>())
            DrawText(dl, rect, e->GetComponent<UITextComponent>(), alpha);
    }
}

} // namespace CHEngine
