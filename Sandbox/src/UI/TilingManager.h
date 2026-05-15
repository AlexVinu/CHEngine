#pragma once
#include "TilingLayout.h"
#include <string>
#include <functional>

namespace Sandbox {

// ─────────────────────────────────────────────────────────────────────────────
// TilingManager
// Owns TilingLayout, handles:
//  • separator drag-resize
//  • panel close / collapse via title-bar buttons
//  • Shift+W popup for adding panels
//  • ghost-rectangle drag-and-drop placement
//  • layout save/load on startup/shutdown
// ─────────────────────────────────────────────────────────────────────────────
class TilingManager {
public:
    explicit TilingManager(const std::string& layoutPath);
    ~TilingManager();

    // Call at the start of each ImGui frame (before drawing panels).
    // Sets up work area rect and recomputes panel rects.
    void BeginFrame(ImVec2 workPos, ImVec2 workSize);

    // Call at the end of each ImGui frame (after all panels drawn).
    // Draws separators, close/collapse buttons, Shift+W popup, ghost.
    void EndFrame();

    // Query per-panel rect (valid between BeginFrame and EndFrame)
    TileRect GetRect(PanelID id)   const { return m_Layout.GetRect(id); }
    bool     IsVisible(PanelID id) const { return m_Layout.IsVisible(id); }
    bool     IsCollapsed(PanelID id) const { return m_Layout.IsCollapsed(id); }

    // Check if any panel changed this frame (e.g. viewport size changed)
    bool LayoutChanged() const { return m_LayoutChanged; }

    // Force a layout reset to defaults
    void ResetLayout();

    // Called from ShiftWMenu when user picks a panel to place
    void StartGhostPlacement(PanelID id) { m_GhostPanel = id; m_DropTarget = {}; }

    // True while user is dragging a new-panel ghost
    bool IsPlacingPanel() const { return m_GhostPanel != PanelID::None; }

private:
    TilingLayout m_Layout;
    std::string  m_SavePath;
    bool         m_LayoutChanged = false;

    // ── Separator drag state ──────────────────────────────────────────────────
    TileNode* m_DragNode      = nullptr;
    bool      m_DragVertical  = false;
    ImVec2    m_DragStart     = {0, 0};
    float     m_DragStartRatio= 0.0f;
    ImVec2    m_DragLineP0    = {0, 0};
    ImVec2    m_DragLineP1    = {0, 0};
    ImVec2    m_WorkPos       = {0, 0};
    ImVec2    m_WorkSize      = {0, 0};

    // ── Ghost placement state ─────────────────────────────────────────────────
    PanelID    m_GhostPanel  = PanelID::None;
    DropTarget m_DropTarget;

    // ── Shift+W popup ─────────────────────────────────────────────────────────
    bool   m_ShiftWOpen = false;
    ImVec2 m_ShiftWPos  = {0, 0};

    // ── Internal draw helpers ─────────────────────────────────────────────────
    void DrawSeparators();
    void DrawPanelOverlay(PanelID id);  // close & collapse buttons on title area
    void DrawGhost();
    void DrawShiftWPopup();
    void HandleShiftW();
};

} // namespace Sandbox
