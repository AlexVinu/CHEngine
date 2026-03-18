#pragma once

#include <imgui.h>
#include <imgui_internal.h>
#include <cstdio>
#include <string>

// ============================================================================
//  RETRO_OS UI Theme  — ver 1.0
//
//  Fully based on RETRO_OS UI KIT Design System:
//
//  COLOR TOKENS
//    SYSTEM_GRAY  #B0B7C4   borders, secondary text, inactive tabs
//    PAPER_WHITE  #F5F5F0   (adapted: used as light surface tint)
//    ACCENT_BLUE  #007BFF   title bars, primary buttons, active tabs
//    ALERT_RED    #DC3545   toasts, destructive actions, error modals
//
//  BORDERS  1px solid, 1px or 4px radius (near-square retro aesthetic)
//
//  TYPE     Monospace throughout — Monaco / Consolas / Courier New
//           ALL_CAPS for section headers
//
//  PANEL HELPERS (same API as UITheme)
//    BeginPanel()    — window with ACCENT_BLUE title bar
//    BeginToolbar()  — locked top bar, retro styling
//    EndPanel()      — ImGui::End()
//    EndToolbar()    — ImGui::End()
//
//  WIDGETS
//    SectionHeader()    — ALL_CAPS label + 1px separator
//    PrimaryButton()    — ACCENT_BLUE filled, square corners
//    SecondaryButton()  — SYSTEM_GRAY bordered, transparent bg
//    DestructiveButton()— ALERT_RED filled
//    Toggle()           — retro checkbox-style toggle
//    SegmentedControl() — tab row  (matches TABS in kit)
//    BeginToast()       — ALERT_RED error banner (TOAST component)
// ============================================================================

namespace UIThemeRetro
{
    // =========================================================================
    //  Color tokens (exact values from the kit)
    // =========================================================================
    static constexpr ImVec4 COL_SYSTEM_GRAY { 0.690f, 0.718f, 0.769f, 1.00f }; // #B0B7C4
    static constexpr ImVec4 COL_PAPER_WHITE { 0.961f, 0.961f, 0.941f, 1.00f }; // #F5F5F0
    static constexpr ImVec4 COL_ACCENT_BLUE { 0.000f, 0.482f, 1.000f, 1.00f }; // #007BFF
    static constexpr ImVec4 COL_ALERT_RED   { 0.863f, 0.208f, 0.271f, 1.00f }; // #DC3545

    // Dark background palette (circuit-board terminal adaptation)
    static constexpr ImVec4 COL_BG0  { 0.047f, 0.055f, 0.078f, 1.00f }; // #0C0E14
    static constexpr ImVec4 COL_BG1  { 0.071f, 0.082f, 0.110f, 1.00f }; // #12151C
    static constexpr ImVec4 COL_BG2  { 0.102f, 0.118f, 0.157f, 1.00f }; // #1A1E28
    static constexpr ImVec4 COL_BG3  { 0.133f, 0.153f, 0.200f, 1.00f }; // #222733
    static constexpr ImVec4 COL_BORDER{ 0.176f, 0.204f, 0.267f, 1.00f }; // #2D3444

    // =========================================================================
    //  Font handles
    // =========================================================================
    inline ImFont* g_FontMono    = nullptr; // primary — monospace body
    inline ImFont* g_FontMonoBig = nullptr; // headings — slightly larger

    inline void PushHeadingFont() { if (g_FontMonoBig) ImGui::PushFont(g_FontMonoBig); }
    inline void PopFont()         { ImGui::PopFont(); }

    // =========================================================================
    //  Layout config  (same fields as UITheme for easy swap)
    // =========================================================================
    struct LayoutConfig
    {
        float toolbarH  = 46.0f;
        float leftFrac  = 0.15f;
        float rightFrac = 0.20f;
        float propsFrac = 0.60f;
    };
    inline LayoutConfig g_Layout;

    // =========================================================================
    //  Font loading — prefers monospace fonts in this order
    // =========================================================================
    static void LoadFonts()
    {
        ImGuiIO& io = ImGui::GetIO();

        // Body monospace
        {
            const char* paths[] = {
                // macOS
                "/System/Library/Fonts/Monaco.ttf",
                "/Library/Fonts/Monaco.ttf",
                "/System/Library/Fonts/Menlo.ttc",
                // Windows
                "C:/Windows/Fonts/consola.ttf",
                "C:/Windows/Fonts/cour.ttf",
                // Linux
                "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
                "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
                "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
                // Custom
                "assets/fonts/JetBrainsMono-Regular.ttf",
            };
            ImFontConfig cfg;
            cfg.OversampleH = 2; cfg.OversampleV = 2;
            for (const char* p : paths)
                if (FILE* f = fopen(p, "rb")) { fclose(f);
                    g_FontMono = io.Fonts->AddFontFromFileTTF(p, 13.0f, &cfg);
                    break; }
        }

        // Heading monospace (slightly bigger)
        {
            const char* paths[] = {
                "/System/Library/Fonts/Monaco.ttf",
                "/Library/Fonts/Monaco.ttf",
                "/System/Library/Fonts/Menlo.ttc",
                "C:/Windows/Fonts/consola.ttf",
                "C:/Windows/Fonts/cour.ttf",
                "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
                "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
                "assets/fonts/JetBrainsMono-Regular.ttf",
            };
            ImFontConfig cfg;
            cfg.OversampleH = 2; cfg.OversampleV = 2;
            for (const char* p : paths)
                if (FILE* f = fopen(p, "rb")) { fclose(f);
                    g_FontMonoBig = io.Fonts->AddFontFromFileTTF(p, 11.0f, &cfg);
                    break; }
        }

        // НЕ вызываем io.Fonts->Build() — см. UITheme.h
        if (!g_FontMono)    g_FontMono    = io.FontDefault;
        if (!g_FontMonoBig) g_FontMonoBig = io.FontDefault;
    }

    // =========================================================================
    //  Apply — sets the full RETRO_OS style
    // =========================================================================
    static void Apply()
    {
        LoadFonts();

        // Use mono font as default
        ImGuiIO& io = ImGui::GetIO();
        if (g_FontMono) io.FontDefault = g_FontMono;

        ImGuiStyle& s = ImGui::GetStyle();
        ImVec4*     c = s.Colors;

        // ── Rounding — near-square as per kit (1–4 px) ───────────────────────
        s.WindowRounding    = 1.0f;
        s.ChildRounding     = 1.0f;
        s.FrameRounding     = 1.0f;
        s.PopupRounding     = 1.0f;
        s.ScrollbarRounding = 1.0f;
        s.GrabRounding      = 1.0f;
        s.TabRounding       = 1.0f;

        // ── Borders — 1px solid as per kit ────────────────────────────────────
        s.WindowBorderSize  = 1.0f;
        s.ChildBorderSize   = 1.0f;
        s.PopupBorderSize   = 1.0f;
        s.FrameBorderSize   = 1.0f;
        s.TabBorderSize     = 1.0f;

        // ── Spacing ───────────────────────────────────────────────────────────
        s.WindowPadding     = ImVec2(10.0f, 8.0f);
        s.FramePadding      = ImVec2(8.0f,  4.0f);
        s.CellPadding       = ImVec2(5.0f,  3.0f);
        s.ItemSpacing       = ImVec2(6.0f,  5.0f);
        s.ItemInnerSpacing  = ImVec2(5.0f,  4.0f);
        s.IndentSpacing     = 16.0f;
        s.ScrollbarSize     = 10.0f;
        s.GrabMinSize       = 12.0f;

        // ── Alignment ────────────────────────────────────────────────────────
        s.WindowTitleAlign         = ImVec2(0.0f, 0.5f);
        s.WindowMenuButtonPosition = ImGuiDir_None;
        s.ButtonTextAlign          = ImVec2(0.5f, 0.5f);

        // ── Shortcuts ─────────────────────────────────────────────────────────
        auto A = [](float r,float g,float b,float a=1.f){ return ImVec4(r,g,b,a); };
        auto fade = [](ImVec4 v, float a){ return ImVec4(v.x,v.y,v.z,a); };

        const ImVec4 bg0    = COL_BG0;
        const ImVec4 bg1    = COL_BG1;
        const ImVec4 bg2    = COL_BG2;
        const ImVec4 bg3    = COL_BG3;
        const ImVec4 bord   = COL_BORDER;
        const ImVec4 acc    = COL_ACCENT_BLUE;   // #007BFF
        const ImVec4 accHi  = A(0.118f,0.580f,1.f);
        const ImVec4 accDim = fade(acc, 0.18f);
        const ImVec4 accMid = fade(acc, 0.40f);
        const ImVec4 text   = A(0.878f,0.906f,0.953f);  // #E0E8F4
        const ImVec4 dim    = A(0.502f,0.557f,0.635f);  // system_gray-ish
        const ImVec4 none   = A(0.f,0.f,0.f,0.f);

        // Titlebar is ACCENT_BLUE per kit WINDOW HEADER component
        const ImVec4 titleActive = acc;
        const ImVec4 titleInact  = A(0.000f, 0.310f, 0.647f);

        c[ImGuiCol_WindowBg]              = bg0;
        c[ImGuiCol_ChildBg]               = bg1;
        c[ImGuiCol_PopupBg]               = A(0.055f, 0.063f, 0.086f, 0.98f);
        c[ImGuiCol_Border]                = bord;
        c[ImGuiCol_BorderShadow]          = none;
        c[ImGuiCol_Text]                  = text;
        c[ImGuiCol_TextDisabled]          = dim;
        c[ImGuiCol_TextSelectedBg]        = accMid;

        // Frame = INPUT_FIELD from kit: dark bg, visible 1px border
        c[ImGuiCol_FrameBg]               = bg2;
        c[ImGuiCol_FrameBgHovered]        = bg3;
        c[ImGuiCol_FrameBgActive]         = fade(acc, 0.22f);

        // Title bar = ACCENT_BLUE per kit
        c[ImGuiCol_TitleBg]               = titleInact;
        c[ImGuiCol_TitleBgActive]         = titleActive;
        c[ImGuiCol_TitleBgCollapsed]      = fade(titleInact, 0.80f);

        c[ImGuiCol_MenuBarBg]             = bg1;

        c[ImGuiCol_ScrollbarBg]           = bg0;
        c[ImGuiCol_ScrollbarGrab]         = bord;
        c[ImGuiCol_ScrollbarGrabHovered]  = dim;
        c[ImGuiCol_ScrollbarGrabActive]   = acc;

        c[ImGuiCol_CheckMark]             = acc;
        c[ImGuiCol_SliderGrab]            = acc;
        c[ImGuiCol_SliderGrabActive]      = accHi;

        // Buttons: PRIMARY = ACCENT_BLUE fill; base = dark bordered
        c[ImGuiCol_Button]                = bg2;
        c[ImGuiCol_ButtonHovered]         = bg3;
        c[ImGuiCol_ButtonActive]          = fade(acc, 0.30f);

        c[ImGuiCol_Header]                = accDim;
        c[ImGuiCol_HeaderHovered]         = fade(acc, 0.30f);
        c[ImGuiCol_HeaderActive]          = accMid;

        c[ImGuiCol_Separator]             = bord;
        c[ImGuiCol_SeparatorHovered]      = acc;
        c[ImGuiCol_SeparatorActive]       = acc;

        c[ImGuiCol_ResizeGrip]            = fade(acc, 0.15f);
        c[ImGuiCol_ResizeGripHovered]     = fade(acc, 0.45f);
        c[ImGuiCol_ResizeGripActive]      = acc;

        // Tabs — matches TABS component: active = ACCENT_BLUE
        c[ImGuiCol_Tab]                   = bg2;
        c[ImGuiCol_TabHovered]            = fade(acc, 0.30f);
        c[ImGuiCol_TabActive]             = acc;
        c[ImGuiCol_TabUnfocused]          = bg1;
        c[ImGuiCol_TabUnfocusedActive]    = A(0.000f, 0.290f, 0.580f);

        c[ImGuiCol_PlotLines]             = acc;
        c[ImGuiCol_PlotLinesHovered]      = accHi;
        c[ImGuiCol_PlotHistogram]         = acc;
        c[ImGuiCol_PlotHistogramHovered]  = accHi;

        c[ImGuiCol_TableHeaderBg]         = bg1;
        c[ImGuiCol_TableBorderStrong]     = bord;
        c[ImGuiCol_TableBorderLight]      = fade(bord, 0.40f);
        c[ImGuiCol_TableRowBg]            = none;
        c[ImGuiCol_TableRowBgAlt]         = A(1,1,1,0.02f);

        c[ImGuiCol_DragDropTarget]        = fade(acc, 0.80f);
        c[ImGuiCol_NavHighlight]          = acc;
        c[ImGuiCol_NavWindowingHighlight] = A(1,1,1,0.70f);
        c[ImGuiCol_NavWindowingDimBg]     = A(0.f,0.f,0.f,0.40f);
        c[ImGuiCol_ModalWindowDimBg]      = A(0.f,0.f,0.f,0.60f);
    }


    // =========================================================================
    //  Panel wrappers  (same API as UITheme)
    // =========================================================================

    // Standard panel — title bar is ACCENT_BLUE (WINDOW HEADER from kit).
    static bool BeginPanel(const char* title,
                            ImVec2 pos,  ImVec2 size,
                            ImGuiWindowFlags extraFlags = 0,
                            bool /*resetSize*/ = false)
    {
        ImGui::SetNextWindowPos (pos,  ImGuiCond_Always);
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);
        // Title bar in ACCENT_BLUE
        ImGui::PushStyleColor(ImGuiCol_TitleBg,       COL_ACCENT_BLUE);
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive,  COL_ACCENT_BLUE);
        ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed,
                              ImVec4(0.000f, 0.310f, 0.647f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1,1,1,1));
        PushHeadingFont();
        bool open = ImGui::Begin(title, nullptr,
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoMove     |
            ImGuiWindowFlags_NoResize   |
            extraFlags);
        PopFont();
        ImGui::PopStyleColor(4);
        // Content text back to normal color
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.878f,0.906f,0.953f,1.f));
        ImGui::PopStyleColor();
        return open;
    }

    static void EndPanel() { ImGui::End(); }

    // Toolbar — dark bar with 1px bottom border accent
    static void BeginToolbar(ImVec2 pos, ImVec2 size)
    {
        ImGui::SetNextWindowPos (pos,  ImGuiCond_Always);
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(10.0f, 8.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg,
            ImVec4(0.039f, 0.047f, 0.067f, 1.0f)); // slightly darker than bg0
        ImGui::PushStyleColor(ImGuiCol_Border, COL_ACCENT_BLUE);
        ImGui::Begin("##toolbar", nullptr,
            ImGuiWindowFlags_NoTitleBar   |
            ImGuiWindowFlags_NoResize     |
            ImGuiWindowFlags_NoMove       |
            ImGuiWindowFlags_NoScrollbar  |
            ImGuiWindowFlags_NoSavedSettings);
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
    }

    static void EndToolbar() { ImGui::End(); }


    // =========================================================================
    //  Custom widgets
    // =========================================================================

    // ALL_CAPS monospace label + 1px separator (matches RETRO_OS section style)
    static void SectionHeader(const char* label)
    {
        ImGui::Spacing();
        PushHeadingFont();
        ImGui::PushStyleColor(ImGuiCol_Text, COL_SYSTEM_GRAY);
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();
        PopFont();
        ImGui::PushStyleColor(ImGuiCol_Separator, COL_BORDER);
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    // ── PRIMARY_BTN — ACCENT_BLUE fill, white text, square corners ───────────
    static bool PrimaryButton(const char* label, ImVec2 size = ImVec2(0,0))
    {
        ImGui::PushStyleColor(ImGuiCol_Button,        COL_ACCENT_BLUE);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.118f,0.580f,1.000f,1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.000f,0.353f,0.784f,1.f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1.f,1.f,1.f,1.f));
        ImGui::PushStyleVar  (ImGuiStyleVar_FrameRounding, 1.0f);
        bool hit = ImGui::Button(label, size);
        ImGui::PopStyleVar  (1);
        ImGui::PopStyleColor(4);
        return hit;
    }

    // ── SECONDARY_BTN — bordered, transparent bg ──────────────────────────────
    static bool SecondaryButton(const char* label, ImVec2 size = ImVec2(0,0))
    {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.176f,0.204f,0.267f,1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.133f,0.153f,0.200f,1.f));
        ImGui::PushStyleColor(ImGuiCol_Text,          COL_SYSTEM_GRAY);
        ImGui::PushStyleColor(ImGuiCol_Border,        COL_SYSTEM_GRAY);
        ImGui::PushStyleVar  (ImGuiStyleVar_FrameRounding, 1.0f);
        ImGui::PushStyleVar  (ImGuiStyleVar_FrameBorderSize, 1.0f);
        bool hit = ImGui::Button(label, size);
        ImGui::PopStyleVar  (2);
        ImGui::PopStyleColor(5);
        return hit;
    }

    // ── DESTRUCTIVE — ALERT_RED ───────────────────────────────────────────────
    static bool DestructiveButton(const char* label, ImVec2 size = ImVec2(0,0))
    {
        ImGui::PushStyleColor(ImGuiCol_Button,        COL_ALERT_RED);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.933f,0.329f,0.376f,1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.706f,0.157f,0.216f,1.f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1.f,1.f,1.f,1.f));
        ImGui::PushStyleVar  (ImGuiStyleVar_FrameRounding, 1.0f);
        bool hit = ImGui::Button(label, size);
        ImGui::PopStyleVar  (1);
        ImGui::PopStyleColor(4);
        return hit;
    }

    // ── CHECKBOX_TOGGLE — retro square style (matches CHECKBOX in kit) ────────
    static bool Toggle(const char* label, bool* v, const char* tooltip = nullptr)
    {
        const float SZ  = 16.0f;
        const float Pad = 2.5f;
        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();

        ImGui::InvisibleButton(label, ImVec2(SZ, SZ));
        bool changed = false;
        if (ImGui::IsItemClicked()) { *v = !*v; changed = true; }
        if (tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("%s", tooltip);

        // Box border
        ImU32 borderCol = *v
            ? IM_COL32(0, 123, 255, 255)    // ACCENT_BLUE
            : IM_COL32(176, 183, 196, 180); // SYSTEM_GRAY

        ImU32 fillCol = *v
            ? IM_COL32(0, 123, 255, 255)
            : IM_COL32(26, 31, 40, 255);    // bg2

        draw->AddRectFilled(pos, ImVec2(pos.x + SZ, pos.y + SZ), fillCol, 1.0f);
        draw->AddRect      (pos, ImVec2(pos.x + SZ, pos.y + SZ), borderCol, 1.0f, 0, 1.0f);

        // Checkmark
        if (*v)
        {
            ImVec2 a { pos.x + Pad + 1.5f, pos.y + SZ * 0.55f };
            ImVec2 b { pos.x + SZ * 0.42f, pos.y + SZ - Pad - 0.5f };
            ImVec2 cc{ pos.x + SZ - Pad,   pos.y + Pad + 0.5f };
            draw->AddLine(a, b,  IM_COL32(255,255,255,255), 1.5f);
            draw->AddLine(b, cc, IM_COL32(255,255,255,255), 1.5f);
        }

        ImGui::SameLine(0.0f, 7.0f);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        return changed;
    }

    // ── TABS (SegmentedControl) — matches TABS component in kit ──────────────
    static bool SegmentedControl(const char* id,
                                  const char** labels, int count,
                                  int* selected,
                                  ImVec2 totalSize = ImVec2(0,0))
    {
        bool changed = false;
        float avail = totalSize.x > 0.0f ? totalSize.x
                                         : ImGui::GetContentRegionAvail().x;
        float btnW = (avail - ImGui::GetStyle().ItemSpacing.x * (count - 1)) / count;

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(1, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 1.0f);

        for (int i = 0; i < count; i++)
        {
            if (i > 0) ImGui::SameLine();
            bool active = (*selected == i);
            if (active) {
                ImGui::PushStyleColor(ImGuiCol_Button,        COL_ACCENT_BLUE);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.118f,0.580f,1.f,1.f));
                ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1,1,1,1));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button,        COL_BG2);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_BG3);
                ImGui::PushStyleColor(ImGuiCol_Text,          COL_SYSTEM_GRAY);
            }
            if (ImGui::Button(
                    (std::string(labels[i]) + "##" + id + std::to_string(i)).c_str(),
                    ImVec2(btnW, totalSize.y)))
            { *selected = i; changed = true; }
            ImGui::PopStyleColor(3);
        }
        ImGui::PopStyleVar(2);
        return changed;
    }

    // ── TOAST / ERROR BANNER — matches TOAST component in kit ────────────────
    // Call inside any window. Pass a short error string.
    // Returns true while visible (call every frame while you want it shown).
    [[maybe_unused]] static void Toast(const char* title, const char* message)
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg,  COL_ALERT_RED);
        ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0.706f,0.157f,0.216f,1.f));
        ImGui::PushStyleColor(ImGuiCol_Text,     ImVec4(1,1,1,1));
        ImGui::PushStyleVar  (ImGuiStyleVar_ChildRounding,    1.0f);
        ImGui::PushStyleVar  (ImGuiStyleVar_ChildBorderSize,  1.0f);
        ImGui::PushStyleVar  (ImGuiStyleVar_WindowPadding,    ImVec2(8,6));

        float w = ImGui::GetContentRegionAvail().x;
        if (ImGui::BeginChild("##toast", ImVec2(w, 0), ImGuiChildFlags_AutoResizeY))
        {
            PushHeadingFont();
            ImGui::TextUnformatted(title);
            PopFont();
            ImGui::SameLine();
            ImGui::TextUnformatted(message);
        }
        ImGui::EndChild();
        ImGui::PopStyleVar  (3);
        ImGui::PopStyleColor(3);
    }

    // ── MODAL — matches MODAL component in kit ────────────────────────────────
    // Usage: if (UIThemeRetro::BeginModal("MODAL")) { ... UIThemeRetro::EndModal(); }
    [[maybe_unused]] static bool BeginModal(const char* title, bool* open = nullptr)
    {
        ImGui::SetNextWindowSize(ImVec2(320, 0), ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_TitleBg,       COL_ACCENT_BLUE);
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive,  COL_ACCENT_BLUE);
        ImGui::PushStyleColor(ImGuiCol_PopupBg,
            ImVec4(0.071f, 0.082f, 0.110f, 1.f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 1.0f);
        PushHeadingFont();
        bool result = ImGui::BeginPopupModal(title, open,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        PopFont();
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(1);
        return result;
    }

    [[maybe_unused]] static void EndModal()
    {
        ImGui::EndPopup();
    }

} // namespace UIThemeRetro
