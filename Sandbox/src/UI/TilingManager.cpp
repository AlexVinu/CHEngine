#include "TilingManager.h"
#include "UIThemeRetroOS.h"
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
    HandleShiftW();
    DrawSeparators();

    // Draw overlay buttons for all visible panels
    for (int i = 1; i < static_cast<int>(PanelID::COUNT); ++i)
        DrawPanelOverlay(static_cast<PanelID>(i));

    DrawGhost();
    DrawShiftWPopup();
}

// ── Separator drag ────────────────────────────────────────────────────────────
void TilingManager::DrawSeparators()
{
    ImDrawList* dl     = ImGui::GetForegroundDrawList();
    ImGuiIO&    io     = ImGui::GetIO();
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
            m_DragNode       = sep.node;
            m_DragVertical   = isV;
            m_DragStart      = io.MousePos;
            m_DragStartRatio = sep.node->ratio;
            m_DragLineP0     = p0;
            m_DragLineP1     = p1;
        }

        // Active drag
        if (m_DragNode == sep.node)
        {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                // Compute new ratio
                float delta, total;
                if (isV)
                {
                    delta = io.MousePos.x - m_DragStart.x;
                    total = m_WorkSize.x; // approximate
                    // find actual total from sep positions
                    total = p1.y - p0.y; // for vertical sep, height is constant
                    // actual width = derived from layout
                    // simple: use delta in pixels / parent width
                    // get parent width from the separator endpoints
                    // We don't have direct access; use stored line coords
                    float parentW = m_WorkSize.x; // fallback
                    // Better: estimate from current rect
                    float curX = m_DragLineP0.x;
                    // Find approximate parent node width
                    // Walk separators to find this specific one
                    for (auto& s2 : seps)
                    {
                        if (s2.node == sep.node && s2.isVertical)
                        {
                            // parent width ≈ distance between childA start and childB end
                            // We can approximate with work size
                            parentW = m_WorkSize.x;
                            break;
                        }
                    }
                    float newRatio = m_DragStartRatio + delta / (parentW * m_DragStartRatio * 2.0f + 1.0f);
                    // Simpler: just track pixel delta relative to line
                    // Ratio change = delta / (available_width)
                    // available_width ≈ (p1 - p0) if horizontal, else use work dims
                    // For vertical separator, available width is the parent node's width
                    // We approximate: use the layout's stored ratio and invert
                    newRatio = m_DragStartRatio + delta / m_WorkSize.x;
                    m_Layout.UpdateRatio(m_DragNode, newRatio);
                    m_Layout.SetWorkArea(m_WorkPos, m_WorkSize);
                    m_Layout.ComputeRects();
                    m_LayoutChanged = true;
                }
                else
                {
                    delta = io.MousePos.y - m_DragStart.y;
                    float newRatio = m_DragStartRatio + delta / m_WorkSize.y;
                    m_Layout.UpdateRatio(m_DragNode, newRatio);
                    m_Layout.SetWorkArea(m_WorkPos, m_WorkSize);
                    m_Layout.ComputeRects();
                    m_LayoutChanged = true;
                }

                // Change cursor
                ImGui::SetMouseCursor(isV ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
            }
            else
            {
                m_DragNode = nullptr; // release
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
}

// ── Panel overlay (close + collapse buttons) ──────────────────────────────────
void TilingManager::DrawPanelOverlay(PanelID id)
{
    if (!m_Layout.IsVisible(id)) return;
    TileRect r = m_Layout.GetRect(id);
    if (!r.valid) return;

    // We draw buttons in the top-right corner of the panel's rect,
    // just above it (in the title bar region of the ImGui window).
    // Since panels use BeginPanel which draws its own title bar,
    // we overlay our close/collapse on the foreground draw list.

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
    if (m_GhostPanel == PanelID::None) return;

    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    // Update drop target
    m_DropTarget = m_Layout.HitTestDrop(io.MousePos);

    // Draw drop preview zone
    if (m_DropTarget.panel != PanelID::None && m_DropTarget.edge != DropEdge::Center
        && m_DropTarget.previewRect.valid)
    {
        ImVec2 pMin = m_DropTarget.previewRect.pos;
        ImVec2 pMax = ImVec2(pMin.x + m_DropTarget.previewRect.size.x,
                             pMin.y + m_DropTarget.previewRect.size.y);
        dl->AddRectFilled(pMin, pMax, IM_COL32(60, 120, 220, 60));
        dl->AddRect(pMin, pMax, IM_COL32(60, 140, 255, 200), 0.0f, 0, 2.0f);
    }

    // Ghost rectangle follows cursor
    const ImVec2 ghostSize = {160.0f, 100.0f};
    ImVec2 ghostMin = ImVec2(io.MousePos.x - ghostSize.x * 0.5f,
                             io.MousePos.y - ghostSize.y * 0.5f);
    ImVec2 ghostMax = ImVec2(ghostMin.x + ghostSize.x, ghostMin.y + ghostSize.y);
    dl->AddRectFilled(ghostMin, ghostMax, IM_COL32(60, 120, 220, 80));
    dl->AddRect(ghostMin, ghostMax, IM_COL32(100, 160, 255, 220), 4.0f, 0, 1.5f);

    // Panel name label on ghost
    const char* name = PanelTitle(m_GhostPanel);
    ImVec2 textSz = ImGui::CalcTextSize(name);
    dl->AddText(ImVec2(io.MousePos.x - textSz.x * 0.5f,
                       io.MousePos.y - textSz.y * 0.5f),
                IM_COL32(220, 230, 255, 255), name);

    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    // Confirm placement on LMB release
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        if (m_DropTarget.panel != PanelID::None && m_DropTarget.edge != DropEdge::None)
        {
            m_Layout.InsertPanel(m_GhostPanel, m_DropTarget.panel, m_DropTarget.edge);
            m_Layout.ComputeRects();
            m_LayoutChanged = true;
            m_Layout.Save(m_SavePath);
        }
        m_GhostPanel = PanelID::None;
        m_DropTarget = {};
    }

    // Cancel on RMB or Escape
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) ||
        ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    {
        m_GhostPanel = PanelID::None;
        m_DropTarget = {};
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
