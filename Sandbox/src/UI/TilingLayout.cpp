#include "TilingLayout.h"

#include <imgui_internal.h>   // ImMax, ImMin, ImClamp
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

namespace Sandbox {

// ── Panel metadata ────────────────────────────────────────────────────────────
const char* PanelName(PanelID id)
{
    switch (id) {
    case PanelID::Viewport:       return "Viewport";
    case PanelID::Inspector:      return "Inspector";
    case PanelID::Properties:     return "Properties";
    case PanelID::ContentBrowser: return "ContentBrowser";
    case PanelID::Profiler:       return "Profiler";
    case PanelID::UVEditor:       return "UVEditor";
    case PanelID::SceneBrowser:   return "SceneBrowser";
    case PanelID::ScriptEditor:   return "ScriptEditor";
    default:                       return "None";
    }
}
const char* PanelTitle(PanelID id)
{
    switch (id) {
    case PanelID::Viewport:       return "Viewport";
    case PanelID::Inspector:      return "Inspector";
    case PanelID::Properties:     return "Properties";
    case PanelID::ContentBrowser: return "Content Browser";
    case PanelID::Profiler:       return "Profiler";
    case PanelID::UVEditor:       return "UV Editor";
    case PanelID::SceneBrowser:   return "Scene Browser";
    case PanelID::ScriptEditor:   return "Script Editor";
    default:                       return "None";
    }
}

// ── Helpers ───────────────────────────────────────────────────────────────────
std::unique_ptr<TileNode> TilingLayout::MakeLeaf(PanelID id)
{
    auto n = std::make_unique<TileNode>();
    n->panel = id;
    return n;
}

std::unique_ptr<TileNode> TilingLayout::MakeSplit(SplitDir dir, float ratio,
                                                    std::unique_ptr<TileNode> a,
                                                    std::unique_ptr<TileNode> b)
{
    auto n = std::make_unique<TileNode>();
    n->splitDir = dir;
    n->ratio    = ratio;
    n->childA   = std::move(a);
    n->childB   = std::move(b);
    return n;
}

// ── Construction / reset ─────────────────────────────────────────────────────
TilingLayout::TilingLayout()
{
    ResetToDefault();
}

void TilingLayout::ResetToDefault()
{
    // Default layout:
    //   Left (78%): Viewport top (75%) | ContentBrowser bottom (25%)
    //   Right (22%): Inspector top (55%) | Properties bottom (45%)
    m_Root = MakeSplit(SplitDir::Vertical, 0.78f,
        // Left column
        MakeSplit(SplitDir::Horizontal, 0.75f,
            MakeLeaf(PanelID::Viewport),
            MakeLeaf(PanelID::ContentBrowser)),
        // Right column
        MakeSplit(SplitDir::Horizontal, 0.55f,
            MakeLeaf(PanelID::Inspector),
            MakeLeaf(PanelID::Properties)));
}

// ── Work area & rect computation ─────────────────────────────────────────────
void TilingLayout::SetWorkArea(ImVec2 pos, ImVec2 size)
{
    m_WorkPos  = pos;
    m_WorkSize = size;
}

void TilingLayout::ComputeRects()
{
    m_Rects.clear();
    m_Separators.clear();
    if (m_Root)
        ComputeNode(m_Root.get(), m_WorkPos, m_WorkSize);
}

// Effective size a node occupies in the given direction (accounting for collapsed leaves)
float TilingLayout::EffectiveSize(TileNode* node, bool horizontal) const
{
    if (!node) return 0.0f;
    if (node->IsLeaf() && node->collapsed)
        return horizontal ? m_WorkSize.x : kTitleBarH;
    return 0.0f; // unknown — caller handles
}

void TilingLayout::ComputeNode(TileNode* node, ImVec2 pos, ImVec2 size)
{
    if (!node) return;

    if (node->IsLeaf())
    {
        TileRect r;
        r.pos   = pos;
        r.size  = size;
        r.valid = (size.x >= 1.0f && size.y >= 1.0f);
        m_Rects[static_cast<int>(node->panel)] = r;
        return;
    }

    // Split node
    const bool isV = (node->splitDir == SplitDir::Vertical); // vertical line → left|right

    // For collapsed leaves we clamp to kTitleBarH along the split axis
    // Simple approach: just apply ratio, collapsed panels have minimum size enforced
    float totalA, totalB;
    if (isV)
    {
        // left | right, ratio = fraction for left
        float w = size.x - kSeparatorW;
        float a = node->childA && node->childA->IsLeaf() && node->childA->collapsed
                  ? kTitleBarH : ImMax(kMinSize, w * node->ratio);
        float b = node->childB && node->childB->IsLeaf() && node->childB->collapsed
                  ? kTitleBarH : ImMax(kMinSize, w - a);
        totalA = a; totalB = b;

        ComputeNode(node->childA.get(), pos, ImVec2(a, size.y));
        // Separator
        {
            SeparatorHit sep;
            sep.node       = node;
            sep.isVertical = true;
            sep.p0 = ImVec2(pos.x + a, pos.y);
            sep.p1 = ImVec2(pos.x + a + kSeparatorW, pos.y + size.y);
            m_Separators.push_back(sep);
        }
        ComputeNode(node->childB.get(), ImVec2(pos.x + a + kSeparatorW, pos.y),
                    ImVec2(b, size.y));
    }
    else
    {
        // top | bottom, ratio = fraction for top
        float h = size.y - kSeparatorW;
        float a = node->childA && node->childA->IsLeaf() && node->childA->collapsed
                  ? kTitleBarH : ImMax(kMinSize, h * node->ratio);
        float b = node->childB && node->childB->IsLeaf() && node->childB->collapsed
                  ? kTitleBarH : ImMax(kMinSize, h - a);
        totalA = a; totalB = b;

        ComputeNode(node->childA.get(), pos, ImVec2(size.x, a));
        {
            SeparatorHit sep;
            sep.node       = node;
            sep.isVertical = false;
            sep.p0 = ImVec2(pos.x, pos.y + a);
            sep.p1 = ImVec2(pos.x + size.x, pos.y + a + kSeparatorW);
            m_Separators.push_back(sep);
        }
        ComputeNode(node->childB.get(), ImVec2(pos.x, pos.y + a + kSeparatorW),
                    ImVec2(size.x, b));
    }
    (void)totalA; (void)totalB;
}

// ── Queries ───────────────────────────────────────────────────────────────────
TileRect TilingLayout::GetRect(PanelID id) const
{
    auto it = m_Rects.find(static_cast<int>(id));
    if (it != m_Rects.end()) return it->second;
    return {};
}

bool TilingLayout::IsVisible(PanelID id) const
{
    return FindNode(m_Root.get(), id) != nullptr;
}

bool TilingLayout::IsCollapsed(PanelID id) const
{
    auto* n = FindNode(m_Root.get(), id);
    return n && n->collapsed;
}

std::vector<PanelID> TilingLayout::VisiblePanels() const
{
    std::vector<PanelID> result;
    for (int i = 1; i < static_cast<int>(PanelID::COUNT); ++i)
        if (IsVisible(static_cast<PanelID>(i)))
            result.push_back(static_cast<PanelID>(i));
    return result;
}

std::vector<PanelID> TilingLayout::ClosedPanels() const
{
    std::vector<PanelID> result;
    for (int i = 1; i < static_cast<int>(PanelID::COUNT); ++i)
        if (!IsVisible(static_cast<PanelID>(i)))
            result.push_back(static_cast<PanelID>(i));
    return result;
}

std::vector<TilingLayout::SeparatorHit> TilingLayout::GetSeparators() const
{
    return m_Separators;
}

// ── Tree search ───────────────────────────────────────────────────────────────
TileNode* TilingLayout::FindNode(TileNode* root, PanelID id) const
{
    if (!root) return nullptr;
    if (root->IsLeaf()) return root->panel == id ? root : nullptr;
    if (auto* r = FindNode(root->childA.get(), id)) return r;
    return FindNode(root->childB.get(), id);
}

TileNode* TilingLayout::FindParent(TileNode* root, TileNode* target) const
{
    if (!root || root->IsLeaf()) return nullptr;
    if (root->childA.get() == target || root->childB.get() == target) return root;
    if (auto* r = FindParent(root->childA.get(), target)) return r;
    return FindParent(root->childB.get(), target);
}

// ── Mutations ─────────────────────────────────────────────────────────────────
void TilingLayout::RemoveLeaf(PanelID id)
{
    TileNode* leaf   = FindNode(m_Root.get(), id);
    if (!leaf) return;
    TileNode* parent = FindParent(m_Root.get(), leaf);
    if (!parent)
    {
        // Root is the leaf — replace with empty
        m_Root.reset();
        return;
    }

    // Promote sibling
    TileNode* grandParent = FindParent(m_Root.get(), parent);
    std::unique_ptr<TileNode> sibling;
    if (parent->childA.get() == leaf)
        sibling = std::move(parent->childB);
    else
        sibling = std::move(parent->childA);

    if (!grandParent)
    {
        m_Root = std::move(sibling);
    }
    else
    {
        if (grandParent->childA.get() == parent)
            grandParent->childA = std::move(sibling);
        else
            grandParent->childB = std::move(sibling);
    }
}

void TilingLayout::ClosePanel(PanelID id)
{
    RemoveLeaf(id);
}

void TilingLayout::ToggleCollapse(PanelID id)
{
    TileNode* n = FindNode(m_Root.get(), id);
    if (n) n->collapsed = !n->collapsed;
}

void TilingLayout::UpdateRatio(TileNode* node, float newRatio)
{
    if (!node) return;
    node->ratio = ImClamp(newRatio, 0.05f, 0.95f);
}

void TilingLayout::InsertPanel(PanelID newPanel, PanelID nearPanel, DropEdge edge)
{
    if (edge == DropEdge::None || edge == DropEdge::Center) return;

    TileNode* near   = FindNode(m_Root.get(), nearPanel);
    TileNode* parent = near ? FindParent(m_Root.get(), near) : nullptr;

    // Determine split direction and which child is new
    SplitDir dir  = (edge == DropEdge::Left || edge == DropEdge::Right)
                    ? SplitDir::Vertical : SplitDir::Horizontal;
    bool newIsA   = (edge == DropEdge::Left || edge == DropEdge::Top);
    float ratio   = newIsA ? 0.35f : 0.65f;

    auto newLeaf  = MakeLeaf(newPanel);
    // Deep-copy near subtree by re-creating it.
    // For the common case (near is a leaf) this is trivial.
    // For split nodes we'd need full deep copy — simplify: only allow inserting next to leaves.
    auto nearCopy = std::make_unique<TileNode>();
    nearCopy->panel     = near->panel;
    nearCopy->collapsed = near->collapsed;
    nearCopy->visible   = near->visible;
    nearCopy->splitDir  = near->splitDir;
    nearCopy->ratio     = near->ratio;
    // Note: childA/childB not copied — we only support inserting next to leaf nodes

    std::unique_ptr<TileNode> splitNode;
    if (newIsA)
        splitNode = MakeSplit(dir, ratio, std::move(newLeaf), std::move(nearCopy));
    else
        splitNode = MakeSplit(dir, ratio, std::move(nearCopy), std::move(newLeaf));

    // Replace near with the new split
    if (!parent)
    {
        m_Root = std::move(splitNode);
    }
    else
    {
        if (parent->childA.get() == near)
            parent->childA = std::move(splitNode);
        else
            parent->childB = std::move(splitNode);
    }
}

// ── Drop target ───────────────────────────────────────────────────────────────
DropTarget TilingLayout::HitTestDrop(ImVec2 mousePos) const
{
    DropTarget result;

    for (int i = 1; i < static_cast<int>(PanelID::COUNT); ++i)
    {
        PanelID pid = static_cast<PanelID>(i);
        TileRect r  = GetRect(pid);
        if (!r.valid) continue;

        ImVec2 min = r.pos;
        ImVec2 max = ImVec2(r.pos.x + r.size.x, r.pos.y + r.size.y);
        if (mousePos.x < min.x || mousePos.x > max.x ||
            mousePos.y < min.y || mousePos.y > max.y) continue;

        // Determine edge from which quarter the mouse is in
        float relX = (mousePos.x - min.x) / r.size.x;
        float relY = (mousePos.y - min.y) / r.size.y;

        const float kEdgeZone = 0.25f;
        DropEdge edge = DropEdge::Center;
        float previewRatio = 0.4f;

        if (relX < kEdgeZone)        edge = DropEdge::Left;
        else if (relX > 1-kEdgeZone) edge = DropEdge::Right;
        else if (relY < kEdgeZone)   edge = DropEdge::Top;
        else if (relY > 1-kEdgeZone) edge = DropEdge::Bottom;

        // Compute preview rect for the new panel
        TileRect preview;
        switch (edge) {
        case DropEdge::Left:
            preview = { min, ImVec2(r.size.x * previewRatio, r.size.y), true };
            break;
        case DropEdge::Right:
            preview = { ImVec2(min.x + r.size.x*(1-previewRatio), min.y),
                        ImVec2(r.size.x * previewRatio, r.size.y), true };
            break;
        case DropEdge::Top:
            preview = { min, ImVec2(r.size.x, r.size.y * previewRatio), true };
            break;
        case DropEdge::Bottom:
            preview = { ImVec2(min.x, min.y + r.size.y*(1-previewRatio)),
                        ImVec2(r.size.x, r.size.y * previewRatio), true };
            break;
        default:
            preview = { min, r.size, true };
            break;
        }

        result.panel       = pid;
        result.edge        = edge;
        result.previewRect = preview;
        return result;
    }
    return result;
}

// ── Persistence ───────────────────────────────────────────────────────────────
// Simple text format: indented tree
//   split V 0.78
//     split H 0.75
//       leaf Viewport
//       leaf ContentBrowser
//     split H 0.55
//       leaf Inspector
//       leaf Properties

void TilingLayout::WriteNode(std::string& out, const TileNode* n, int depth) const
{
    std::string indent(depth * 2, ' ');
    if (!n) return;
    if (n->IsLeaf())
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%sleaf %s %d\n",
                      indent.c_str(), PanelName(n->panel),
                      n->collapsed ? 1 : 0);
        out += buf;
    }
    else
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%ssplit %c %.4f\n",
                      indent.c_str(),
                      n->splitDir == SplitDir::Vertical ? 'V' : 'H',
                      n->ratio);
        out += buf;
        WriteNode(out, n->childA.get(), depth + 1);
        WriteNode(out, n->childB.get(), depth + 1);
    }
}

void TilingLayout::Save(const std::string& path) const
{
    std::string text;
    WriteNode(text, m_Root.get(), 0);
    std::ofstream f(path);
    if (f.is_open()) f << text;
}

static const char* SkipSpaces(const char* p)
{
    while (*p == ' ' || *p == '\t') ++p;
    return p;
}

static const char* ReadLine(const char* p, std::string& line)
{
    line.clear();
    while (*p && *p != '\n') line += *p++;
    if (*p == '\n') ++p;
    return p;
}

bool TilingLayout::ParseNode(const char*& cur, std::unique_ptr<TileNode>& out) const
{
    // skip empty lines
    while (*cur == '\n' || *cur == '\r') ++cur;
    if (!*cur) return false;

    const char* lineStart = cur;
    std::string line;
    cur = ReadLine(cur, line);

    const char* p = SkipSpaces(line.c_str());
    if (std::strncmp(p, "leaf", 4) == 0)
    {
        p += 4;
        p = SkipSpaces(p);
        // read name
        std::string name;
        while (*p && *p != ' ' && *p != '\t') name += *p++;
        p = SkipSpaces(p);
        int col = 0;
        std::sscanf(p, "%d", &col);

        PanelID pid = PanelID::None;
        for (int i = 1; i < static_cast<int>(PanelID::COUNT); ++i)
            if (name == PanelName(static_cast<PanelID>(i)))
            { pid = static_cast<PanelID>(i); break; }

        out = MakeLeaf(pid);
        out->collapsed = col != 0;
        return true;
    }
    else if (std::strncmp(p, "split", 5) == 0)
    {
        p += 5;
        p = SkipSpaces(p);
        char dirC = *p++;
        p = SkipSpaces(p);
        float ratio = 0.5f;
        std::sscanf(p, "%f", &ratio);

        auto node = std::make_unique<TileNode>();
        node->splitDir = (dirC == 'V') ? SplitDir::Vertical : SplitDir::Horizontal;
        node->ratio    = ratio;

        if (!ParseNode(cur, node->childA)) return false;
        if (!ParseNode(cur, node->childB)) return false;
        out = std::move(node);
        return true;
    }
    return false;
}

bool TilingLayout::Load(const std::string& path)
{
    std::ifstream f(path);
    if (!f.is_open()) return false;
    std::ostringstream ss; ss << f.rdbuf();
    std::string text = ss.str();

    const char* cur = text.c_str();
    std::unique_ptr<TileNode> root;
    if (ParseNode(cur, root))
    {
        m_Root = std::move(root);
        return true;
    }
    return false;
}

} // namespace Sandbox
