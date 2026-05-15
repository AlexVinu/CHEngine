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

    // Programmatically insert a panel without ghost interaction
    void InsertPanel(PanelID newPanel, PanelID nearPanel, DropEdge edge)
    {
        m_Layout.InsertPanel(newPanel, nearPanel, edge);
        m_LayoutChanged = true;
    }

    // Apply a named layout preset: "script_focus" | "model_focus" | "default"
    void ApplyPreset(const std::string& name)
    {
        m_Layout.ApplyPreset(name);
        m_LayoutChanged = true;
    }

    // Called from ShiftWMenu when user picks a panel to place
    void StartGhostPlacement(PanelID id)
    {
        m_GhostPanel        = id;
        m_DragFromPanel     = PanelID::None; // from popup, not title bar
        m_GhostWaitingPlace = false;
        m_DropTarget        = {};
    }

    // True while user is dragging a new-panel ghost
    bool IsPlacingPanel() const { return m_GhostPanel != PanelID::None; }

    // Call this INSIDE each panel's Begin/End to draw close+collapse buttons
    // into the panel's own draw list (so they appear above content, below popups)
    void DrawOverlayForPanel(PanelID id);

private:
    TilingLayout m_Layout;
    std::string  m_SavePath;
    bool         m_LayoutChanged = false;

    // ── Separator drag state ──────────────────────────────────────────────────
    TileNode* m_DragNode        = nullptr;
    bool      m_DragVertical    = false;
    ImVec2    m_DragStart       = {0, 0};
    float     m_DragStartRatio  = 0.0f;
    float     m_DragParentSpan  = 1.0f;   // available size along drag axis (no separator)
    float     m_DragParentOrigin= 0.0f;   // left (V) or top (H) edge of parent node
    ImVec2    m_DragLineP0      = {0, 0};
    ImVec2    m_DragLineP1      = {0, 0};
    ImVec2    m_WorkPos       = {0, 0};
    ImVec2    m_WorkSize      = {0, 0};

    // ── Ghost placement state ─────────────────────────────────────────────────
    PanelID    m_GhostPanel        = PanelID::None;
    PanelID    m_DragFromPanel     = PanelID::None; // set when dragging from title bar
    bool       m_GhostWaitingPlace = false;         // skip first frame after popup click
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
