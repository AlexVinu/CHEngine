#include "TilingManager.h"
#include "UIThemeRetroOS.h"
#include "EditorPopupState.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <cstdio>

namespace Sandbox {

static constexpr float kSepHover   = 6.0f;  // hover detection half-width
static constexpr float kOverlayH   = 22.0f; // panel overlay bar height

// ── Construction / destruction ────────────────────────────────────────────────
TilingManager::TilingManager(const std::string& layoutPath)
    : m_SavePath(layoutPath)
{
    if (!m_Layout.Load(layoutPath))
        m_Layout.ResetToDefault();
}

TilingManager::~TilingManager()
{
    m_Layout.Save(m_SavePath);
}

// ── Per-frame ─────────────────────────────────────────────────────────────────
void TilingManager::BeginFrame(ImVec2 workPos, ImVec2 workSize)
{
    m_LayoutChanged = false;
    m_WorkPos       = workPos;
    m_WorkSize      = workSize;
    m_Layout.SetWorkArea(workPos, workSize);
    m_Layout.ComputeRects();
}

void TilingManager::EndFrame()
{
    DrawSeparators();

    // Draw close/collapse buttons for every visible panel
    for (int i = 1; i < static_cast<int>(PanelID::COUNT); ++i)
        DrawOverlayForPanel(static_cast<PanelID>(i));

    DrawGhost();
    // Shift+W popup is drawn by SceneViewLayer_ShiftWMenu::Draw() (called from ImGuiFrame)
}

// ── Separator drag ────────────────────────────────────────────────────────────
void TilingManager::DrawSeparators()
{
    // Use a transparent passthrough window so separators render BELOW popups
    // (GetForegroundDrawList renders above everything including popups)
    ImGui::SetNextWindowPos(m_WorkPos,  ImGuiCond_Always);
    ImGui::SetNextWindowSize(m_WorkSize,ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    const ImGuiWindowFlags kOvFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoResize     |
        ImGuiWindowFlags_NoScrollbar| ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoFocusOnAppearing    | ImGuiWindowFlags_NoMouseInputs;
    ImGui::Begin("##tiling_sep_overlay", nullptr, kOvFlags);
    ImGui::PopStyleVar();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGuiIO&    io = ImGui::GetIO();
    const ImU32 colSep = IM_COL32(60, 60, 65, 255);
    const ImU32 colHov = IM_COL32(100, 140, 220, 200);
    const ImU32 colDrag= IM_COL32(100, 140, 220, 255);

    auto seps = m_Layout.GetSeparators();

    for (auto& sep : seps)
    {
        bool isV = sep.isVertical; // vertical sep line → drag horizontally
        ImVec2 p0 = sep.p0, p1 = sep.p1;

        // Hit rect
        ImVec2 hMin = isV
            ? ImVec2(p0.x - kSepHover, p0.y)
            : ImVec2(p0.x, p0.y - kSepHover);
        ImVec2 hMax = isV
            ? ImVec2(p1.x + kSepHover, p1.y)
            : ImVec2(p1.x, p1.y + kSepHover);

        bool hovered = (io.MousePos.x >= hMin.x && io.MousePos.x <= hMax.x &&
                        io.MousePos.y >= hMin.y && io.MousePos.y <= hMax.y);

        // Start drag
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !m_DragNode)
        {
            m_DragNode         = sep.node;
            m_DragVertical     = isV;
            m_DragStart        = io.MousePos;
            m_DragStartRatio   = sep.node->ratio;
            m_DragParentSpan   = sep.parentSpan;
            m_DragParentOrigin = sep.parentOrigin;
            m_DragLineP0       = p0;
            m_DragLineP1       = p1;
        }

        // Active drag
        if (m_DragNode == sep.node)
        {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                // Correct formula:
                // newRatio = (mousePos - parentOrigin) / parentSpan
                // This directly maps the separator position to a ratio,
                // regardless of nesting depth or workspace size.
                float newRatio;
                if (isV)
                {
                    float sepX   = m_DragLineP0.x + (io.MousePos.x - m_DragStart.x);
                    float childA = sepX - m_DragParentOrigin;
                    newRatio = childA / ImMax(m_DragParentSpan, 1.0f);
                }
                else
                {
                    float sepY   = m_DragLineP0.y + (io.MousePos.y - m_DragStart.y);
                    float childA = sepY - m_DragParentOrigin;
                    newRatio = childA / ImMax(m_DragParentSpan, 1.0f);
                }

                m_Layout.UpdateRatio(m_DragNode, newRatio);
                m_Layout.SetWorkArea(m_WorkPos, m_WorkSize);
                m_Layout.ComputeRects();
                m_LayoutChanged = true;

                ImGui::SetMouseCursor(isV ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
            }
            else
            {
                m_DragNode = nullptr;
                m_Layout.Save(m_SavePath);
            }
        }

        // Draw separator line
        ImU32 col = (m_DragNode == sep.node) ? colDrag
                  : hovered                  ? colHov
                  : colSep;

        if (isV)
            dl->AddRectFilled(p0, p1, col);
        else
            dl->AddRectFilled(p0, p1, col);
    }
    ImGui::End(); // ##tiling_sep_overlay
}

// ── Panel overlay (close + collapse buttons) ──────────────────────────────────
// Draw close + collapse buttons for a panel.
// Uses GetForegroundDrawList() so buttons are always visible above content.
// Hidden while any popup is open (popup is on top anyway).
void TilingManager::DrawOverlayForPanel(PanelID id)
{
    // Don't draw over popups — they handle their own Z-order
    if (EditorPopup::AnyOpen()) return;

    if (!m_Layout.IsVisible(id)) return;
    TileRect r = m_Layout.GetRect(id);
    if (!r.valid) return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImGuiIO&    io = ImGui::GetIO();

    // Position our buttons inside the panel title bar
    // The panel title bar is the top kOverlayH pixels of the rect
    const float btnSize = 14.0f;
    const float margin  = 4.0f;

    // Close button: top-right
    ImVec2 closeMax = ImVec2(r.pos.x + r.size.x - margin,
                              r.pos.y + kOverlayH * 0.5f + btnSize * 0.5f);
    ImVec2 closeMin = ImVec2(closeMax.x - btnSize, closeMax.y - btnSize);

    // Collapse button: left of close
    ImVec2 colMax = ImVec2(closeMin.x - margin, closeMax.y);
    ImVec2 colMin = ImVec2(colMax.x - btnSize, colMax.y - btnSize);

    bool closeHov = (io.MousePos.x >= closeMin.x && io.MousePos.x <= closeMax.x &&
                     io.MousePos.y >= closeMin.y && io.MousePos.y <= closeMax.y);
    bool colHov   = (io.MousePos.x >= colMin.x   && io.MousePos.x <= colMax.x   &&
                     io.MousePos.y >= colMin.y   && io.MousePos.y <= colMax.y);

    // Draw close ×
    ImU32 closeBg = closeHov ? IM_COL32(200, 60, 60, 220) : IM_COL32(80, 80, 85, 160);
    dl->AddRectFilled(closeMin, closeMax, closeBg, 3.0f);
    float cx = (closeMin.x + closeMax.x) * 0.5f;
    float cy = (closeMin.y + closeMax.y) * 0.5f;
    float d  = btnSize * 0.28f;
    dl->AddLine(ImVec2(cx-d,cy-d), ImVec2(cx+d,cy+d), IM_COL32(255,255,255,230), 1.5f);
    dl->AddLine(ImVec2(cx+d,cy-d), ImVec2(cx-d,cy+d), IM_COL32(255,255,255,230), 1.5f);

    // Draw collapse ^/v
    bool collapsed = m_Layout.IsCollapsed(id);
    ImU32 colBg = colHov ? IM_COL32(60, 120, 200, 220) : IM_COL32(80, 80, 85, 160);
    dl->AddRectFilled(colMin, colMax, colBg, 3.0f);
    float mx = (colMin.x + colMax.x) * 0.5f;
    float my = (colMin.y + colMax.y) * 0.5f;
    float hh = btnSize * 0.25f;
    float hw = btnSize * 0.32f;
    if (!collapsed)
    {
        // Up arrow (collapse)
        dl->AddTriangleFilled(
            ImVec2(mx, my - hh),
            ImVec2(mx - hw, my + hh),
            ImVec2(mx + hw, my + hh),
            IM_COL32(255,255,255,230));
    }
    else
    {
        // Down arrow (expand)
        dl->AddTriangleFilled(
            ImVec2(mx, my + hh),
            ImVec2(mx - hw, my - hh),
            ImVec2(mx + hw, my - hh),
            IM_COL32(255,255,255,230));
    }

    // Handle clicks
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemActive())
    {
        if (closeHov)
        {
            m_Layout.ClosePanel(id);
            m_Layout.ComputeRects();
            m_LayoutChanged = true;
            m_Layout.Save(m_SavePath);
        }
        else if (colHov)
        {
            m_Layout.ToggleCollapse(id);
            m_Layout.ComputeRects();
            m_LayoutChanged = true;
            m_Layout.Save(m_SavePath);
        }
    }
}

// ── Ghost placement ───────────────────────────────────────────────────────────
void TilingManager::DrawGhost()
{
    // ── Detect title-bar drag → start ghost ──────────────────────────────────
    // Check if LMB was just pressed in a panel title bar (outside close/collapse buttons)
    if (m_GhostPanel == PanelID::None && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        ImGuiIO& io2 = ImGui::GetIO();
        for (int i = 1; i < static_cast<int>(PanelID::COUNT); ++i)
        {
            PanelID pid = static_cast<PanelID>(i);
            if (!m_Layout.IsVisible(pid)) continue;
            TileRect r = m_Layout.GetRect(pid);
            if (!r.valid) continue;

            // Title bar zone = top kOverlayH pixels of the panel
            ImVec2 tbMin = r.pos;
            ImVec2 tbMax = ImVec2(r.pos.x + r.size.x, r.pos.y + kOverlayH);

            // Exclude right-side buttons area
            float btnAreaX = r.pos.x + r.size.x - kOverlayH * 2.0f;

            if (io2.MousePos.x >= tbMin.x && io2.MousePos.x < btnAreaX &&
                io2.MousePos.y >= tbMin.y && io2.MousePos.y <= tbMax.y)
            {
                // Start dragging this panel
                m_GhostPanel         = pid;
                m_DragFromPanel      = pid;
                m_GhostWaitingPlace  = false;
                m_DropTarget         = {};
                break;
            }
        }
    }

    if (m_GhostPanel == PanelID::None) return;

    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    // Update drop target every frame
    m_DropTarget = m_Layout.HitTestDrop(io.MousePos);

    // ── Draw drop preview zone ────────────────────────────────────────────────
    bool layoutEmpty = m_Layout.VisiblePanels().empty();

    if (layoutEmpty)
    {
        // Empty workspace — highlight the whole area as drop zone
        ImVec2 pMin = m_WorkPos;
        ImVec2 pMax = ImVec2(m_WorkPos.x + m_WorkSize.x, m_WorkPos.y + m_WorkSize.y);
        dl->AddRectFilled(pMin, pMax, IM_COL32(60, 120, 220, 30));
        dl->AddRect(pMin, pMax, IM_COL32(80, 150, 255, 180), 0.0f, 0, 2.0f);
    }
    else if (m_DropTarget.panel != PanelID::None
             && m_DropTarget.panel != m_GhostPanel
             && m_DropTarget.edge  != DropEdge::Center
             && m_DropTarget.previewRect.valid)
    {
        ImVec2 pMin = m_DropTarget.previewRect.pos;
        ImVec2 pMax = ImVec2(pMin.x + m_DropTarget.previewRect.size.x,
                             pMin.y + m_DropTarget.previewRect.size.y);
        dl->AddRectFilled(pMin, pMax, IM_COL32(60, 120, 220, 55));
        dl->AddRect(pMin, pMax, IM_COL32(80, 150, 255, 220), 0.0f, 0, 2.0f);
    }

    // ── Ghost rectangle follows cursor ────────────────────────────────────────
    const ImVec2 ghostSize = {160.0f, 90.0f};
    ImVec2 gMin = ImVec2(io.MousePos.x - ghostSize.x * 0.5f,
                          io.MousePos.y - ghostSize.y * 0.5f);
    ImVec2 gMax = ImVec2(gMin.x + ghostSize.x, gMin.y + ghostSize.y);
    dl->AddRectFilled(gMin, gMax, IM_COL32(50, 110, 210, 70));
    dl->AddRect(gMin, gMax, IM_COL32(100, 160, 255, 230), 5.0f, 0, 2.0f);

    // Panel label
    const char* name = PanelTitle(m_GhostPanel);
    ImVec2 textSz = ImGui::CalcTextSize(name);
    float tx = io.MousePos.x - textSz.x * 0.5f;
    float ty = io.MousePos.y - textSz.y * 0.5f;
    // Shadow
    dl->AddText(ImVec2(tx+1, ty+1), IM_COL32(0,0,0,140), name);
    dl->AddText(ImVec2(tx,   ty  ), IM_COL32(220, 235, 255, 255), name);

    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    // ── Confirm placement on LMB click (NOT the same click that started) ──────
    // For "from popup" mode: m_GhostWaitingPlace = true means first click already used
    // For "title-bar drag" mode: confirm on LMB RELEASE after dragging
    bool confirm = false;
    bool cancel  = false;

    if (m_DragFromPanel != PanelID::None)
    {
        // Title-bar drag: place on mouse release
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
            confirm = true;
    }
    else
    {
        // From popup: skip the frame when popup was clicked (mouse just released),
        // then confirm on the next LMB click anywhere.
        if (!m_GhostWaitingPlace)
            m_GhostWaitingPlace = true;
        else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            confirm = true;
    }

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)
        || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        cancel = true;

    if (confirm)
    {
        bool placed = false;

        if (m_DropTarget.panel != PanelID::None
            && m_DropTarget.panel != m_GhostPanel
            && m_DropTarget.edge  != DropEdge::None
            && m_DropTarget.edge  != DropEdge::Center)
        {
            // If dragging from title bar: remove from old position first
            if (m_DragFromPanel != PanelID::None)
                m_Layout.ClosePanel(m_DragFromPanel);

            m_Layout.InsertPanel(m_GhostPanel, m_DropTarget.panel, m_DropTarget.edge);
            m_Layout.ComputeRects();
            m_LayoutChanged = true;
            m_Layout.Save(m_SavePath);
            placed = true;
        }
        else if (m_Layout.VisiblePanels().empty())
        {
            // Layout is empty — place as the only panel (becomes root)
            m_Layout.InsertFirstPanel(m_GhostPanel);
            m_Layout.ComputeRects();
            m_LayoutChanged = true;
            m_Layout.Save(m_SavePath);
            placed = true;
        }
        (void)placed;

        if (m_DragFromPanel != PanelID::None && !placed)
        {
            // Dropped nowhere useful — panel stays where it was
        }
        m_GhostPanel        = PanelID::None;
        m_DragFromPanel     = PanelID::None;
        m_GhostWaitingPlace = false;
        m_DropTarget        = {};
    }
    else if (cancel)
    {
        m_GhostPanel        = PanelID::None;
        m_DragFromPanel     = PanelID::None;
        m_GhostWaitingPlace = false;
        m_DropTarget        = {};
    }
}

// ── Shift+W popup ─────────────────────────────────────────────────────────────
void TilingManager::HandleShiftW()
{
    ImGuiIO& io = ImGui::GetIO();
    if (m_GhostPanel == PanelID::None && !m_ShiftWOpen &&
        io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_W, false))
    {
        m_ShiftWOpen = true;
        m_ShiftWPos  = ImGui::GetMousePos();
    }
}

void TilingManager::DrawShiftWPopup()
{
    if (!m_ShiftWOpen) return;

    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
        (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
         !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)))
    {
        m_ShiftWOpen = false;
        return;
    }

    static const ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoTitleBar       |
        ImGuiWindowFlags_NoResize         |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoMove           |
        ImGuiWindowFlags_NoSavedSettings  |
        ImGuiWindowFlags_NoScrollbar      |
        ImGuiWindowFlags_NoNav;

    ImGui::SetNextWindowPos(m_ShiftWPos, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.97f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(10.0f, 8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 7.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,    ImVec2(8.0f, 3.0f));
    ImGui::Begin("##tiling_add_panel", nullptr, kFlags);
    ImGui::PopStyleVar(3);

    const float w = 160.0f;

    UIThemeRetro::PushHeadingFont();
    ImGui::TextDisabled("Add Window  [Shift+W]");
    UIThemeRetro::PopFont();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 5.0f));

    auto addEntry = [&](PanelID id)
    {
        const char* label = PanelTitle(id);
        bool inLayout = m_Layout.IsVisible(id);

        // Dim if already visible
        if (inLayout)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));

        char buf[64];
        snprintf(buf, sizeof(buf), "  %s%s", label, inLayout ? " (open)" : "");
        if (ImGui::Selectable(buf, false, inLayout ? ImGuiSelectableFlags_Disabled : 0,
                              ImVec2(w, 0)))
        {
            // Start ghost drag
            m_GhostPanel  = id;
            m_DropTarget  = {};
            m_ShiftWOpen  = false;
        }

        if (inLayout)
            ImGui::PopStyleColor();
    };

    addEntry(PanelID::Viewport);
    addEntry(PanelID::Inspector);
    addEntry(PanelID::Properties);
    addEntry(PanelID::ContentBrowser);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    addEntry(PanelID::Profiler);
    addEntry(PanelID::UVEditor);
    addEntry(PanelID::SceneBrowser);
    addEntry(PanelID::ScriptEditor);

    ImGui::PopStyleVar();
    ImGui::End();
}

void TilingManager::ResetLayout()
{
    m_Layout.ResetToDefault();
    m_Layout.ComputeRects();
    m_LayoutChanged = true;
    m_Layout.Save(m_SavePath);
}

} // namespace Sandbox
