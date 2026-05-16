#pragma once

#include <CHEngine/Utils/AppPaths.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <filesystem>

// ============================================================================
//  CHEngine UI Theme — Dark / Apple-inspired
//
//  FONTS
//    g_FontBody    — SF Pro Display Regular  14.5 px  (all regular text)
//    g_FontHeading — PP Neue Montreal Mono Bold  11 px  (titles, labels)
//
//  LAYOUT CONFIG
//    g_Layout  — one struct to control all panel sizes/positions.
//                Edit its fields to reshape the entire editor layout.
//
//  PANEL HELPERS
//    BeginPanel()  / EndPanel()   — window with heading font in title bar
//    BeginToolbar()/ EndToolbar() — locked top bar, no title
//
//  WIDGETS
//    Toggle()           iOS-style switch
//    PrimaryButton()    blue filled pill button
//    DestructiveButton()red tinted pill button
//    SectionHeader()    small all-caps PP Neue label + separator
// ============================================================================
namespace UITheme
{
    // =========================================================================
    //  Font handles (set by Apply(), use via PushHeadingFont / PopFont)
    // =========================================================================
    inline ImFont* g_FontBody    = nullptr;
    inline ImFont* g_FontHeading = nullptr;


    inline void PushHeadingFont() { if (g_FontHeading) ImGui::PushFont(g_FontHeading); }
    inline void PopFont()         { ImGui::PopFont(); }

    // =========================================================================
    //  Layout configuration
    //  ── Change these values to reshape the entire editor ──
    // =========================================================================
    struct LayoutConfig
    {
        // All sizes are FRACTIONS of the window, so nothing ever goes off-screen.
        float toolbarH  = 46.0f;   // toolbar height in px (only fixed value)
        float leftFrac  = 0.15f;   // left panel width   as fraction of W  (e.g. 0.15 = 15 %)
        float rightFrac = 0.20f;   // right column width as fraction of W
        float propsFrac = 0.60f;   // Properties height  as fraction of side-column height
        // ─── Room to grow ─────────────────────────────────────────────────────
        // float bottomFrac = 0.18f; // future: console / asset browser
        // float leftFrac2  = 0.12f; // future: second left column
    };
    inline LayoutConfig g_Layout;   // <─ edit this to change the layout


    // =========================================================================
    //  Font loading
    // =========================================================================
    static void LoadFonts()
    {
        ImGuiIO& io = ImGui::GetIO();

        // ── Body font (platform-aware search order) ───────────────────────
        {
            const char* paths[] = {
                // macOS — SF Pro (system + user-installed locations)
                "/System/Library/Fonts/SFNS.ttf",
                "/System/Library/Fonts/SFNSText.ttf",
                "/System/Library/Fonts/SFNSDisplay.ttf",
                "/System/Library/Fonts/SF-Pro.ttf",
                "/Library/Fonts/SF-Pro-Display-Regular.otf",
                "/Library/Fonts/SF-Pro-Text-Regular.otf",
                "/Library/Fonts/SF-Pro.ttf",
                // macOS fallbacks
                "/System/Library/Fonts/HelveticaNeue.ttc",
                "/System/Library/Fonts/Helvetica.ttc",
                // Windows — Segoe UI (closest to SF Pro)
                "C:/Windows/Fonts/segoeui.ttf",
                "C:/Windows/Fonts/arial.ttf",
                // Linux
                "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                "/usr/share/fonts/TTF/DejaVuSans.ttf",
            };
            ImFontConfig cfg;
            cfg.OversampleH = 3;  cfg.OversampleV = 2;
            for (const char* p : paths)
                if (std::filesystem::exists(p)) {
                    g_FontBody = io.Fonts->AddFontFromFileTTF(p, 14.5f, &cfg);
                    break; }
        }

        // ── Headings: PP Neue Montreal Mono Bold ──────────────────────────
        {
            const std::string headingFont =
                (CHEngine::AppPaths::EditorAssetsDir() / "fonts" / "PPNeueMontrealMono-Bold.otf").string();
            ImFontConfig cfg;
            cfg.OversampleH = 3;  cfg.OversampleV = 2;
            cfg.GlyphOffset  = ImVec2(0.0f, 1.0f);
            if (std::filesystem::exists(headingFont))
                g_FontHeading = io.Fonts->AddFontFromFileTTF(headingFont.c_str(), 11.0f, &cfg);
        }

        // НЕ вызываем io.Fonts->Build() — современные бэкенды (Metal)
        // с ImGuiBackendFlags_RendererHasTextures строят атлас автоматически.
        // Legacy бэкенды (OpenGL) тоже строят при первом GetTexDataAsRGBA32().
        if (!g_FontBody)    g_FontBody    = io.FontDefault;
        if (!g_FontHeading) g_FontHeading = io.FontDefault;
    }

    // =========================================================================
    //  Style (colours, rounding, spacing)
    // =========================================================================
    static void Apply()
    {
        LoadFonts();

        ImGuiStyle& s = ImGui::GetStyle();
        ImVec4*     c = s.Colors;

        // ── Rounding ─────────────────────────────────────────────────────────
        s.WindowRounding    = 10.0f;
        s.ChildRounding     =  8.0f;
        s.FrameRounding     = 20.0f;   // pill inputs & buttons
        s.PopupRounding     = 10.0f;
        s.ScrollbarRounding =  6.0f;
        s.GrabRounding      = 20.0f;   // pill slider grab
        s.TabRounding       =  6.0f;
        s.LogSliderDeadzone =  4.0f;

        // ── Borders ───────────────────────────────────────────────────────────
        s.WindowBorderSize  = 1.0f;
        s.ChildBorderSize   = 0.0f;
        s.PopupBorderSize   = 1.0f;
        s.FrameBorderSize   = 0.0f;
        s.TabBorderSize     = 0.0f;

        // ── Spacing ───────────────────────────────────────────────────────────
        s.WindowPadding     = ImVec2(14.0f, 12.0f);
        s.FramePadding      = ImVec2(12.0f,  6.0f);
        s.CellPadding       = ImVec2( 6.0f,  4.0f);
        s.ItemSpacing       = ImVec2( 8.0f,  6.0f);
        s.ItemInnerSpacing  = ImVec2( 6.0f,  4.0f);
        s.IndentSpacing     = 18.0f;
        s.ScrollbarSize     =  8.0f;
        s.GrabMinSize       = 16.0f;

        // ── Alignment ────────────────────────────────────────────────────────
        s.WindowTitleAlign         = ImVec2(0.0f, 0.5f);
        s.WindowMenuButtonPosition = ImGuiDir_None;
        s.ButtonTextAlign          = ImVec2(0.5f, 0.5f);
        s.SelectableTextAlign      = ImVec2(0.0f, 0.5f);

        // ── Palette ───────────────────────────────────────────────────────────
        const ImVec4 bg0    { 0.110f, 0.110f, 0.118f, 1.00f }; // #1C1C1E
        const ImVec4 bg1    { 0.173f, 0.173f, 0.180f, 1.00f }; // #2C2C2E
        const ImVec4 bg2    { 0.227f, 0.227f, 0.235f, 1.00f }; // #3A3A3C
        const ImVec4 bg3    { 0.282f, 0.282f, 0.290f, 1.00f }; // #48484A
        const ImVec4 bord   { 0.282f, 0.282f, 0.290f, 0.55f };
        const ImVec4 acc    { 0.039f, 0.518f, 1.000f, 1.00f }; // #0A84FF
        const ImVec4 accHi  { 0.251f, 0.612f, 1.000f, 1.00f };
        const ImVec4 accDim { 0.039f, 0.518f, 1.000f, 0.22f };
        const ImVec4 accMid { 0.039f, 0.518f, 1.000f, 0.45f };
        const ImVec4 text   { 1.000f, 1.000f, 1.000f, 1.00f };
        const ImVec4 dim    { 0.557f, 0.557f, 0.576f, 1.00f }; // #8E8E93
        const ImVec4 none   { 0.000f, 0.000f, 0.000f, 0.00f };

        auto fade = [](ImVec4 v, float a){ return ImVec4(v.x,v.y,v.z,a); };

        c[ImGuiCol_WindowBg]              = bg0;
        c[ImGuiCol_ChildBg]               = bg1;
        c[ImGuiCol_PopupBg]               = ImVec4(0.145f,0.145f,0.153f,0.98f);
        c[ImGuiCol_Border]                = bord;
        c[ImGuiCol_BorderShadow]          = none;
        c[ImGuiCol_Text]                  = text;
        c[ImGuiCol_TextDisabled]          = dim;
        c[ImGuiCol_TextSelectedBg]        = accMid;
        c[ImGuiCol_FrameBg]               = bg2;
        c[ImGuiCol_FrameBgHovered]        = bg3;
        c[ImGuiCol_FrameBgActive]         = fade(acc, 0.30f);
        c[ImGuiCol_TitleBg]               = bg0;
        c[ImGuiCol_TitleBgActive]         = ImVec4(0.130f,0.130f,0.140f,1.00f);
        c[ImGuiCol_TitleBgCollapsed]      = fade(bg0, 0.90f);
        c[ImGuiCol_MenuBarBg]             = bg1;
        c[ImGuiCol_ScrollbarBg]           = none;
        c[ImGuiCol_ScrollbarGrab]         = bg3;
        c[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.376f,0.376f,0.384f,1.00f);
        c[ImGuiCol_ScrollbarGrabActive]   = acc;
        c[ImGuiCol_CheckMark]             = acc;
        c[ImGuiCol_SliderGrab]            = acc;
        c[ImGuiCol_SliderGrabActive]      = accHi;
        c[ImGuiCol_Button]                = bg2;
        c[ImGuiCol_ButtonHovered]         = fade(acc, 0.75f);
        c[ImGuiCol_ButtonActive]          = acc;
        c[ImGuiCol_Header]                = accDim;
        c[ImGuiCol_HeaderHovered]         = fade(acc, 0.35f);
        c[ImGuiCol_HeaderActive]          = accMid;
        c[ImGuiCol_Separator]             = fade(bg3, 0.55f);
        c[ImGuiCol_SeparatorHovered]      = accMid;
        c[ImGuiCol_SeparatorActive]       = acc;
        c[ImGuiCol_ResizeGrip]            = fade(acc, 0.18f);
        c[ImGuiCol_ResizeGripHovered]     = fade(acc, 0.55f);
        c[ImGuiCol_ResizeGripActive]      = fade(acc, 0.85f);
        c[ImGuiCol_Tab]                   = ImVec4(0.155f,0.155f,0.162f,1.00f);
        c[ImGuiCol_TabHovered]            = fade(acc, 0.35f);
        c[ImGuiCol_TabActive]             = fade(acc, 0.55f);
        c[ImGuiCol_TabUnfocused]          = bg0;
        c[ImGuiCol_TabUnfocusedActive]    = bg1;
        c[ImGuiCol_PlotLines]             = acc;
        c[ImGuiCol_PlotLinesHovered]      = accHi;
        c[ImGuiCol_PlotHistogram]         = acc;
        c[ImGuiCol_PlotHistogramHovered]  = accHi;
        c[ImGuiCol_TableHeaderBg]         = bg1;
        c[ImGuiCol_TableBorderStrong]     = bg3;
        c[ImGuiCol_TableBorderLight]      = fade(bg3,0.40f);
        c[ImGuiCol_TableRowBg]            = none;
        c[ImGuiCol_TableRowBgAlt]         = ImVec4(1,1,1,0.025f);
        c[ImGuiCol_DragDropTarget]        = fade(acc, 0.85f);
        c[ImGuiCol_NavHighlight]          = acc;
        c[ImGuiCol_NavWindowingHighlight] = ImVec4(1,1,1,0.70f);
        c[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.8f,0.8f,0.8f,0.20f);
        c[ImGuiCol_ModalWindowDimBg]      = ImVec4(0,0,0,0.55f);
    }


    // =========================================================================
    //  Panel wrappers
    //  ─── Title bar renders in g_FontHeading automatically ────────────────
    // =========================================================================

    // Standard panel.
    // Both position AND size are ALWAYS enforced every frame, computed from
    // window fractions — so panels always fit the window regardless of imgui.ini.
    // The unused resetSize parameter is kept for API compatibility.
    static bool BeginPanel(const char* title,
                            ImVec2 pos,  ImVec2 size,
                            ImGuiWindowFlags extraFlags = 0,
                            bool /*resetSize*/ = false)
    {
        ImGui::SetNextWindowPos (pos,  ImGuiCond_Always);
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);
        PushHeadingFont();
        bool open = ImGui::Begin(title, nullptr,
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoMove     |
            ImGuiWindowFlags_NoResize   |
            extraFlags);
        PopFont();  // content drawn in body font

        // ── Gradient title bar overlay ────────────────────────────────────────
        // Drawn after Begin() so it overlays ImGui's flat TitleBg colour.
        if (open)
        {
            ImDrawList* dl  = ImGui::GetWindowDrawList();
            ImVec2      wp  = ImGui::GetWindowPos();
            float       tbH = ImGui::GetCurrentWindow()->TitleBarHeight;
            ImVec2      tl  = wp;
            ImVec2      br  = ImVec2(wp.x + ImGui::GetWindowWidth(), wp.y + tbH);
            float       r   = ImGui::GetStyle().WindowRounding;

            // Rounded-top gradient (matches window rounding)
            dl->AddRectFilledMultiColor(
                ImVec2(tl.x, tl.y + r), br,
                IM_COL32(52,  95, 180, 210),   // left-top:  mid blue
                IM_COL32(52,  95, 180, 210),   // right-top
                IM_COL32(22,  48, 110, 210),   // right-bottom: deep blue
                IM_COL32(22,  48, 110, 210));  // left-bottom

            // Top rounded cap (same colour as top row)
            dl->AddRectFilled(tl, ImVec2(br.x, tl.y + r),
                IM_COL32(52, 95, 180, 210), r,
                ImDrawFlags_RoundCornersTop);

            // Thin accent line at bottom of title bar
            dl->AddLine(ImVec2(tl.x, br.y - 1), ImVec2(br.x, br.y - 1),
                IM_COL32(80, 140, 255, 80), 1.0f);

        }

        return open;
    }

    static void EndPanel() { ImGui::End(); }

    // Locked top toolbar — no title bar, no resize/move.
    static void BeginToolbar(ImVec2 pos, ImVec2 size)
    {
        ImGui::SetNextWindowPos (pos,  ImGuiCond_Always);
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);
        // Use standard WindowPadding.y so vertical centering is consistent
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(14.0f, 9.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg,
            ImVec4(0.130f, 0.130f, 0.140f, 1.0f));
        ImGui::Begin("##toolbar", nullptr,
            ImGuiWindowFlags_NoTitleBar   |
            ImGuiWindowFlags_NoResize     |
            ImGuiWindowFlags_NoMove       |
            ImGuiWindowFlags_NoScrollbar  |
            ImGuiWindowFlags_NoSavedSettings);
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(1);
    }

    static void EndToolbar() { ImGui::End(); }


    // =========================================================================
    //  Custom widgets
    // =========================================================================

    // ── Section label in PP Neue Montreal Mono Bold ───────────────────────────
    static void SectionHeader(const char* label)
    {
        ImGui::Spacing();
        PushHeadingFont();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();
        PopFont();
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.282f, 0.282f, 0.290f, 0.45f));
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    // ── iOS Toggle switch ─────────────────────────────────────────────────────
    static bool Toggle(const char* label, bool* v, const char* tooltip = nullptr)
    {
        const float W=36.f, H=20.f, R=H*0.5f, Pad=2.2f;
        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();

        ImGui::InvisibleButton(label, ImVec2(W, H));
        bool changed = false;
        if (ImGui::IsItemClicked()) { *v = !*v; changed = true; }
        if (tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("%s", tooltip);

        ImU32 track = *v
            ? (ImGui::IsItemHovered() ? IM_COL32(64,156,255,255) : IM_COL32(10,132,255,255))
            : (ImGui::IsItemHovered() ? IM_COL32(90,90,92,255)   : IM_COL32(72,72,74,255));

        draw->AddRectFilled(pos, ImVec2(pos.x+W, pos.y+H), track, R);

        float t     = *v ? 1.0f : 0.0f;
        float knobX = pos.x + Pad + R + t*(W-H);
        float knobY = pos.y + R;
        draw->AddCircleFilled({knobX, knobY}, R-Pad, IM_COL32(255,255,255,255));
        draw->AddCircle      ({knobX, knobY}, R-Pad, IM_COL32(0,0,0,20), 0, 1.2f);

        ImGui::SameLine(0.0f, 9.0f);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        return changed;
    }

    // ── Primary (blue filled) button ──────────────────────────────────────────
    static bool PrimaryButton(const char* label, ImVec2 size = ImVec2(0,0))
    {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.039f,0.518f,1.00f,1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.251f,0.612f,1.00f,1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.039f,0.380f,0.85f,1.00f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1.00f, 1.00f, 1.00f,1.00f));
        bool hit = ImGui::Button(label, size);
        ImGui::PopStyleColor(4);
        return hit;
    }

    // ── Destructive (red tint) button ─────────────────────────────────────────
    static bool DestructiveButton(const char* label, ImVec2 size = ImVec2(0,0))
    {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(1.00f,0.231f,0.188f,0.15f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.00f,0.231f,0.188f,0.28f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1.00f,0.231f,0.188f,1.00f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1.00f,0.231f,0.188f,1.00f));
        bool hit = ImGui::Button(label, size);
        ImGui::PopStyleColor(4);
        return hit;
    }

    // ── Segmented control (radio-button row) ──────────────────────────────────
    // Returns true if selection changed.
    // Usage:
    //   static int sel = 0;
    //   const char* opts[] = {"A","B","C"};
    //   UITheme::SegmentedControl("##id", opts, 3, &sel, ImVec2(-1,0));
    static bool SegmentedControl(const char* id,
                                  const char** labels, int count,
                                  int* selected,
                                  ImVec2 totalSize = ImVec2(0,0))
    {
        bool changed = false;
        float avail = totalSize.x > 0.0f ? totalSize.x
                                         : ImGui::GetContentRegionAvail().x;
        float btnW  = (avail - ImGui::GetStyle().ItemSpacing.x * (count-1)) / count;

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2,0));
        for (int i = 0; i < count; i++)
        {
            if (i > 0) ImGui::SameLine();
            bool active = (*selected == i);
            if (active) {
                ImGui::PushStyleColor(ImGuiCol_Button,
                    ImVec4(0.039f,0.518f,1.000f,1.00f));
                ImGui::PushStyleColor(ImGuiCol_Text,
                    ImVec4(1.0f,1.0f,1.0f,1.0f));
            }
            if (ImGui::Button((std::string(labels[i])+"##"+id+std::to_string(i)).c_str(),
                              ImVec2(btnW, totalSize.y)))
            { *selected = i; changed = true; }
            if (active) ImGui::PopStyleColor(2);
        }
        ImGui::PopStyleVar();
        return changed;
    }

} // namespace UITheme
