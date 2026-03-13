#pragma once

#include "UITheme.h"
#include "UIThemeRetroOS.h"

// ============================================================================
//  UIActive — thin dispatcher that routes all theme calls to whichever
//  theme is currently selected.  Switch at runtime via SetTheme().
//
//  Usage:
//    UIActive::SetTheme(AppTheme::Dark);      // or AppTheme::RetroOS
//    UIActive::BeginPanel(...);
//    UIActive::EndPanel();
// ============================================================================

enum class AppTheme : int { Dark = 0, RetroOS = 1 };

namespace UIActive
{
    inline AppTheme g_Theme = AppTheme::RetroOS;

    inline void SetTheme(AppTheme t)
    {
        g_Theme = t;
        if (t == AppTheme::RetroOS)
            UIThemeRetro::Apply();
        else
            UITheme::Apply();
    }

    // ── Layout ────────────────────────────────────────────────────────────────
    struct LayoutProxy
    {
        float toolbarH  = 46.0f;
        float leftFrac  = 0.15f;
        float rightFrac = 0.20f;
        float propsFrac = 0.60f;

        void Sync()
        {
            if (UIActive::g_Theme == AppTheme::RetroOS) {
                toolbarH  = UIThemeRetro::g_Layout.toolbarH;
                leftFrac  = UIThemeRetro::g_Layout.leftFrac;
                rightFrac = UIThemeRetro::g_Layout.rightFrac;
                propsFrac = UIThemeRetro::g_Layout.propsFrac;
            } else {
                toolbarH  = UITheme::g_Layout.toolbarH;
                leftFrac  = UITheme::g_Layout.leftFrac;
                rightFrac = UITheme::g_Layout.rightFrac;
                propsFrac = UITheme::g_Layout.propsFrac;
            }
        }
    };
    inline LayoutProxy g_Layout;

    inline void SyncLayout() { g_Layout.Sync(); }

    // ── Panel / Toolbar ────────────────────────────────────────────────────────
    inline bool BeginPanel(const char* title, ImVec2 pos, ImVec2 size,
                           ImGuiWindowFlags flags = 0, bool resetSize = false)
    {
        if (g_Theme == AppTheme::RetroOS)
            return UIThemeRetro::BeginPanel(title, pos, size, flags, resetSize);
        return UITheme::BeginPanel(title, pos, size, flags, resetSize);
    }

    inline void EndPanel()
    {
        if (g_Theme == AppTheme::RetroOS) UIThemeRetro::EndPanel();
        else                              UITheme::EndPanel();
    }

    inline void BeginToolbar(ImVec2 pos, ImVec2 size)
    {
        if (g_Theme == AppTheme::RetroOS) UIThemeRetro::BeginToolbar(pos, size);
        else                              UITheme::BeginToolbar(pos, size);
    }

    inline void EndToolbar()
    {
        if (g_Theme == AppTheme::RetroOS) UIThemeRetro::EndToolbar();
        else                              UITheme::EndToolbar();
    }

    // ── Widgets ────────────────────────────────────────────────────────────────
    inline void SectionHeader(const char* label)
    {
        if (g_Theme == AppTheme::RetroOS) UIThemeRetro::SectionHeader(label);
        else                              UITheme::SectionHeader(label);
    }

    inline bool Toggle(const char* label, bool* v, const char* tooltip = nullptr)
    {
        if (g_Theme == AppTheme::RetroOS) return UIThemeRetro::Toggle(label, v, tooltip);
        return UITheme::Toggle(label, v, tooltip);
    }

    inline bool PrimaryButton(const char* label, ImVec2 size = ImVec2(0, 0))
    {
        if (g_Theme == AppTheme::RetroOS) return UIThemeRetro::PrimaryButton(label, size);
        return UITheme::PrimaryButton(label, size);
    }

    inline bool SecondaryButton(const char* label, ImVec2 size = ImVec2(0, 0))
    {
        if (g_Theme == AppTheme::RetroOS) return UIThemeRetro::SecondaryButton(label, size);
        return ImGui::Button(label, size);  // UITheme has no SecondaryButton
    }

    inline bool DestructiveButton(const char* label, ImVec2 size = ImVec2(0, 0))
    {
        if (g_Theme == AppTheme::RetroOS) return UIThemeRetro::DestructiveButton(label, size);
        return UITheme::DestructiveButton(label, size);
    }

    inline bool SegmentedControl(const char* id, const char** labels, int count,
                                  int* selected, ImVec2 size = ImVec2(0, 0))
    {
        if (g_Theme == AppTheme::RetroOS)
            return UIThemeRetro::SegmentedControl(id, labels, count, selected, size);
        return UITheme::SegmentedControl(id, labels, count, selected, size);
    }
} // namespace UIActive
