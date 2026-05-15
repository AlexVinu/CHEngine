#pragma once
#include <imgui.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

namespace Sandbox {

// ── Panel identifiers ─────────────────────────────────────────────────────────
enum class PanelID : uint8_t {
    None          = 0,
    Viewport      = 1,
    Inspector     = 2,
    Properties    = 3,
    ContentBrowser= 4,
    Profiler      = 5,
    UVEditor      = 6,
    SceneBrowser  = 7,
    ScriptEditor  = 8,
    COUNT
};

const char* PanelName(PanelID id);   // "Viewport", "Inspector", …
const char* PanelTitle(PanelID id);  // display title

// ── Layout tree ───────────────────────────────────────────────────────────────

// Split direction: the line that divides two children
// Vertical   → left | right  (vertical dividing line)
// Horizontal → top  | bottom (horizontal dividing line)
enum class SplitDir : uint8_t { Vertical, Horizontal };

struct TileNode {
    // ── Leaf ──────────────────────────────────────────────────────────────────
    PanelID  panel     = PanelID::None;
    bool     collapsed = false;   // only title bar visible
    bool     visible   = true;    // panel exists in layout

    // ── Split ─────────────────────────────────────────────────────────────────
    SplitDir splitDir = SplitDir::Vertical;
    float    ratio    = 0.5f;     // fraction for childA (0..1)
    std::unique_ptr<TileNode> childA;
    std::unique_ptr<TileNode> childB;

    bool IsLeaf() const { return !childA; }
};

// ── Computed rect for a panel ─────────────────────────────────────────────────
struct TileRect {
    ImVec2 pos  = {0, 0};
    ImVec2 size = {0, 0};
    bool   valid = false;
};

// ── Drop zone when dragging a new panel ───────────────────────────────────────
enum class DropEdge : uint8_t { None, Left, Right, Top, Bottom, Center };
struct DropTarget {
    PanelID  panel = PanelID::None;
    DropEdge edge  = DropEdge::None;
    TileRect previewRect;        // where the new panel would appear
};

// ═════════════════════════════════════════════════════════════════════════════
// TilingLayout — pure data/logic, no ImGui draw calls
// ═════════════════════════════════════════════════════════════════════════════
class TilingLayout {
public:
    static constexpr float kTitleBarH  = 26.0f;  // collapsed panel height
    static constexpr float kSeparatorW = 4.0f;   // draggable separator width
    static constexpr float kMinSize    = 60.0f;  // minimum panel dimension

    TilingLayout();

    // ── Frame setup ───────────────────────────────────────────────────────────
    void SetWorkArea(ImVec2 pos, ImVec2 size);
    void ComputeRects();

    // ── Panel queries ─────────────────────────────────────────────────────────
    TileRect  GetRect(PanelID id)       const;
    bool      IsVisible(PanelID id)     const;
    bool      IsCollapsed(PanelID id)   const;
    std::vector<PanelID> VisiblePanels() const;
    std::vector<PanelID> ClosedPanels()  const; // panels not in tree

    // ── Mutations (call before ComputeRects) ──────────────────────────────────
    void ClosePanel(PanelID id);
    void ToggleCollapse(PanelID id);
    // Insert newPanel by splitting the node that contains nearPanel.
    // edge: which side of nearPanel the new panel appears on.
    void InsertPanel(PanelID newPanel, PanelID nearPanel, DropEdge edge);
    // Direct ratio update from separator drag
    void UpdateRatio(TileNode* node, float newRatio);

    // ── Separator hit-testing ─────────────────────────────────────────────────
    // Returns separator node and whether drag is along X (vertical sep) or Y
    struct SeparatorHit {
        TileNode* node = nullptr;
        bool      isVertical = false; // true → horizontal drag changes ratio
        ImVec2    p0, p1;             // separator line endpoints
    };
    std::vector<SeparatorHit> GetSeparators() const;

    // ── Drop target for ghost placement ──────────────────────────────────────
    DropTarget HitTestDrop(ImVec2 mousePos) const;

    // ── Persistence ───────────────────────────────────────────────────────────
    void Save(const std::string& path) const;
    bool Load(const std::string& path);
    void ResetToDefault();

    // Raw tree access (for drag handling)
    TileNode* Root() { return m_Root.get(); }

private:
    std::unique_ptr<TileNode> m_Root;
    ImVec2 m_WorkPos  = {0, 0};
    ImVec2 m_WorkSize = {0, 0};

    // Computed rects (filled by ComputeRects)
    std::unordered_map<int, TileRect> m_Rects;
    std::vector<SeparatorHit>         m_Separators;

    // ── Internal helpers ──────────────────────────────────────────────────────
    void  ComputeNode(TileNode* node, ImVec2 pos, ImVec2 size);
    float EffectiveSize(TileNode* node, bool horizontal) const;

    TileNode*  FindNode(TileNode* root, PanelID id) const;
    TileNode*  FindParent(TileNode* root, TileNode* target) const;
    void       RemoveLeaf(PanelID id);
    // Build default tree
    static std::unique_ptr<TileNode> MakeLeaf(PanelID id);
    static std::unique_ptr<TileNode> MakeSplit(SplitDir dir, float ratio,
                                                std::unique_ptr<TileNode> a,
                                                std::unique_ptr<TileNode> b);
    // Serialization helpers
    void      WriteNode(std::string& out, const TileNode* node, int depth) const;
    bool      ParseNode(const char*& cur, std::unique_ptr<TileNode>& out) const;
};

} // namespace Sandbox
