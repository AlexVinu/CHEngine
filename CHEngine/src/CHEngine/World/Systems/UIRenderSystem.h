#pragma once
#include "CHEngine/World/ISystem.h"
#include <unordered_map>
#include <string>

// Forward declarations
struct ImFont;
struct ImDrawList;

namespace CHEngine {

class World;
class DeferredOps;
class Timestep;

// ─────────────────────────────────────────────────────────────────────────────
// UIRenderSystem
//
// Renders screen-space UI (UICanvasComponent, UIRectTransformComponent, etc.)
// using ImGui's draw list. Runs in the Presentation phase AFTER the scene,
// so UI is always drawn on top.
//
// Font management: fonts are loaded from TTF/OTF files and cached by path+size.
// ─────────────────────────────────────────────────────────────────────────────
class UIRenderSystem : public ISystem
{
public:
    UIRenderSystem()
        : ISystem(SystemPhase::Presentation, 200)  // after RenderSystem (priority 10)
    {}

    const char* GetName() const override { return "UIRenderSystem"; }

    void Run(World& world, DeferredOps& deferred, Timestep ts) override;

    // Clear font cache (call when font atlas is rebuilt)
    void ClearFontCache() { m_FontCache.clear(); }

private:
    // Resolved pixel rect for a UI element given screen size
    struct UIRect {
        float x, y, w, h;  // pixels, top-left origin
    };

    UIRect ResolveRect(const struct UIRectTransformComponent& rt,
                       float screenW, float screenH) const;

    // Font cache: "path:size" → ImFont*
    // ImFont* is owned by ImGui font atlas — we just keep a pointer.
    ImFont* GetFont(const std::string& path, float size);

    // Draw individual component types
    void DrawPanel (ImDrawList* dl, const UIRect& r, const struct UIPanelComponent&  c, float alpha);
    void DrawImage (ImDrawList* dl, const UIRect& r, const struct UIImageComponent&  c, float alpha);
    void DrawText  (ImDrawList* dl, const UIRect& r, const struct UITextComponent&   c, float alpha);
    void DrawButton(ImDrawList* dl, const UIRect& r, const struct UIButtonComponent& c,
                    float alpha, bool hovered, bool pressed);
    void DrawSlider(ImDrawList* dl, const UIRect& r, const struct UISliderComponent& c, float alpha);

    // Hit-test for interactive elements (returns true if mouse is inside rect)
    bool IsHovered(const UIRect& r) const;

    std::unordered_map<std::string, ImFont*> m_FontCache;
};

} // namespace CHEngine
