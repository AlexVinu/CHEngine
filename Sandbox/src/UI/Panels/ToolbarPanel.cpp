#include "ToolbarPanel.h"

#include "EditorViewport.h"
#include "SetTransformCommand.h"

#include <CHEngine/Application.h>
#include <CHEngine/EngineConfig.h>
#include <CHEngine/Project/ProjectManager.h>
#include <CHEngine/Utils/FileDialog.h>

#include "UIThemeActive.h"

#include <cstdio>
#include <filesystem>

namespace Sandbox {

ToolbarPanel::ToolbarPanel()
{
    LoadIcons();
}

void ToolbarPanel::LoadIcons()
{
    m_IconTranslate = CHEngine::RenderFacade::CreateTextureFromFile("editor_assets/icons/icon_translate.png");
    m_IconRotate    = CHEngine::RenderFacade::CreateTextureFromFile("editor_assets/icons/icon_rotate.png");
    m_IconScale     = CHEngine::RenderFacade::CreateTextureFromFile("editor_assets/icons/icon_scale.png");
    m_IconsLoaded   = m_IconTranslate.IsValid() && m_IconRotate.IsValid() && m_IconScale.IsValid();
}

// Возвращает ImTextureID из TextureHandle (работает для Metal и OGL)
static ImTextureID ToImTex(CHEngine::TextureHandle h)
{
    if (!h.IsValid()) return static_cast<ImTextureID>(0);
    auto* f = CHEngine::RenderFacade::GetRenderFactory();
    if (!f) return static_cast<ImTextureID>(0);
    return static_cast<ImTextureID>(f->GetTextureNativeID(h));
}

void ToolbarPanel::Draw(EditorUiHost& host, ImVec2 pos, ImVec2 size)
{
    // Ленивая загрузка: при первом Draw factory точно уже готова
    if (!m_IconsLoaded)
        LoadIcons();
    UIActive::BeginToolbar(pos, size);
    SceneSession& activeSession = host.GetActiveSceneSession();

    const float winH = ImGui::GetWindowHeight();
    const float startY = ImGui::GetCursorPosY();
    const float centerY = startY + (winH - 18.0f) * 0.5f;

    struct VC {
        float cy;
        void operator()(float itemH) const { ImGui::SetCursorPosY(cy - itemH * 0.5f); }
    } vcenter{ centerY };

    // ── Keyboard shortcuts (T/R/S, Cmd+Z, Cmd+P, Escape) ─────────────────────
    if (!ImGui::GetIO().WantTextInput)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_T, false))
            host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
                [&host] { host.GetGizmoOperation() = ImGuizmo::TRANSLATE; }, [] {}, false));
        if (ImGui::IsKeyPressed(ImGuiKey_R, false))
            host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
                [&host] { host.GetGizmoOperation() = ImGuizmo::ROTATE; }, [] {}, false));
        if (ImGui::IsKeyPressed(ImGuiKey_S, false))
            host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
                [&host] { host.GetGizmoOperation() = ImGuizmo::SCALE; }, [] {}, false));
        if (ImGui::IsKeyPressed(ImGuiKey_F3, false))
            host.GetShowProfiler() = !host.GetShowProfiler();

        const bool undoMod = ImGui::GetIO().KeySuper || ImGui::GetIO().KeyAlt;
        if (undoMod && ImGui::IsKeyPressed(ImGuiKey_Z, false))
            host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
                [&host] { host.RequestUndo(); }, [] {}, false));

        const bool playMod = ImGui::GetIO().KeySuper || ImGui::GetIO().KeyCtrl;
        if (playMod && ImGui::IsKeyPressed(ImGuiKey_P, false))
        {
            host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
                [&host] {
                    SceneSession& s = host.GetActiveSceneSession();
                    if (s.SessionState == SceneSession::State::Edit)        host.EnterPlayMode();
                    else if (s.SessionState == SceneSession::State::Play)   host.EnterPauseMode();
                    else if (s.SessionState == SceneSession::State::Pause)  host.ResumeFromPause();
                }, [] {}, false));
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) &&
            activeSession.SessionState != SceneSession::State::Edit)
        {
            host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
                [&host] { host.StopPlayMode(); }, [] {}, false));
        }
    }

    // ── Session navigation (left side) ────────────────────────────────────────
    {
        vcenter(ImGui::GetFrameHeight());
        ImGui::BeginDisabled(host.GetActiveSessionIndex() == 0);
        if (ImGui::ArrowButton("##prev", ImGuiDir_Left))
            host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
                [&host] { host.SetActiveSessionIndex(host.GetActiveSessionIndex() - 1); }, [] {}, false));
        ImGui::EndDisabled();

        ImGui::SameLine(0, 4);
        vcenter(ImGui::GetTextLineHeight());
        {
            const auto& sessions = host.GetSceneSessions();
            const size_t idx = host.GetActiveSessionIndex();
            const std::string name = (idx < sessions.size()) ? sessions[idx].DisplayName() : "?";
            ImGui::Text("%s  %u/%u", name.c_str(),
                        static_cast<uint32_t>(idx + 1),
                        static_cast<uint32_t>(sessions.size()));
        }

        ImGui::SameLine(0, 4);
        vcenter(ImGui::GetFrameHeight());
        ImGui::BeginDisabled(host.GetActiveSessionIndex() + 1 >= host.GetSceneSessions().size());
        if (ImGui::ArrowButton("##next", ImGuiDir_Right))
            host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
                [&host] { host.SetActiveSessionIndex(host.GetActiveSessionIndex() + 1); }, [] {}, false));
        ImGui::EndDisabled();

        ImGui::SameLine(0, 6);
        vcenter(ImGui::GetFrameHeight());
        ImGui::BeginDisabled(host.GetSceneSessions().size() <= 1);
        if (ImGui::Button("x##close"))
            host.CloseSceneSession(host.GetActiveSessionIndex());
        ImGui::EndDisabled();
    }

    // ── Gizmo mode icons (Translate / Rotate / Scale) ─────────────────────────
    {
        ImGui::SameLine(0, 12);

        const ImGuizmo::OPERATION curOp = host.GetGizmoOperation();
        const float iconSize = 22.0f;   // размер кнопки (иконка 32x32 масштабируется)
        const ImVec2 btnSize(iconSize, iconSize);

        struct GizmoBtn {
            ImGuizmo::OPERATION op;
            CHEngine::TextureHandle tex;
            const char* fallback; // текст если иконка не загружена
            const char* tooltip;
        } btns[] = {
            { ImGuizmo::TRANSLATE, m_IconTranslate, "T", "Translate (T)" },
            { ImGuizmo::ROTATE,    m_IconRotate,    "R", "Rotate (R)"    },
            { ImGuizmo::SCALE,     m_IconScale,     "S", "Scale (S)"     },
        };

        for (auto& btn : btns)
        {
            const bool active = (curOp == btn.op);

            // Подсветка активной кнопки
            if (active)
            {
                ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.26f, 0.59f, 0.98f, 0.80f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.59f, 0.98f, 1.00f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.06f, 0.39f, 0.78f, 1.00f));
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.12f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1.0f, 1.0f, 1.0f, 0.25f));
            }

            // Убираем границу кнопки
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1, 1));
            vcenter(btnSize.y + 2.0f);

            bool clicked = false;
            ImTextureID texId = ToImTex(btn.tex);
            if (texId)
            {
                // Иконка загружена — рисуем как ImageButton
                // Metal: UV (0,0)→(1,1); OGL: перевёрнуто (0,1)→(1,0)
                const bool isMetal = (CHEngine::Application::Get().GetRenderAPIType() == CHEngine::ERenderAPI::METAL);
                ImVec2 uv0 = isMetal ? ImVec2(0,0) : ImVec2(0,1);
                ImVec2 uv1 = isMetal ? ImVec2(1,1) : ImVec2(1,0);
                // Иконки белые — tint для активного: ярко, для неактивного: тускло
                ImVec4 tint = active ? ImVec4(1,1,1,1) : ImVec4(0.75f, 0.75f, 0.75f, 0.85f);
                clicked = ImGui::ImageButton(btn.fallback, texId, btnSize, uv0, uv1,
                                             ImVec4(0,0,0,0), tint);
            }
            else
            {
                // Fallback: текстовая кнопка
                clicked = ImGui::Button(btn.fallback, ImVec2(iconSize, iconSize));
            }

            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);

            if (clicked)
            {
                const ImGuizmo::OPERATION newOp = btn.op;
                host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
                    [&host, newOp] { host.GetGizmoOperation() = newOp; }, [] {}, false));
            }

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", btn.tooltip);

            ImGui::SameLine(0, 2);
        }
    }

    // ── Play / Stop (centered) ────────────────────────────────────────────────
    {
        const bool isEdit  = (activeSession.SessionState == SceneSession::State::Edit);
        const bool isPlay  = (activeSession.SessionState == SceneSession::State::Play);
        const bool isPause = (activeSession.SessionState == SceneSession::State::Pause);

        const float pad    = ImGui::GetStyle().FramePadding.x;
        const float playW  = ImGui::CalcTextSize(isPlay ? "Pause" : (isPause ? "Resume" : "Play")).x + pad * 2.0f + 14.0f;
        const float stopW  = ImGui::CalcTextSize("Stop").x + pad * 2.0f + 14.0f;
        const float gap    = 4.0f;
        const float blockW = playW + stopW + gap;

        float screenX = ImGui::GetWindowPos().x + (ImGui::GetWindowWidth() - blockW) * 0.5f;
        float screenY = ImGui::GetWindowPos().y + (winH - ImGui::GetFrameHeight()) * 0.5f;
        ImGui::SetCursorScreenPos(ImVec2(screenX, screenY));

        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.15f, 0.50f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.62f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.10f, 0.38f, 0.10f, 1.0f));
        if (isPlay)
        {
            if (ImGui::Button("Pause##pc", ImVec2(playW, 0)))
                host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
                    [&host] { host.EnterPauseMode(); }, [] {}, false));
        }
        else
        {
            const char* lbl = isPause ? "Resume##pc" : "Play##pc";
            if (ImGui::Button(lbl, ImVec2(playW, 0)))
                host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
                    [&host, isEdit] {
                        if (isEdit) host.EnterPlayMode(); else host.ResumeFromPause();
                    }, [] {}, false));
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine(0, gap);
        ImGui::BeginDisabled(isEdit);
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.50f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.62f, 0.20f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.38f, 0.10f, 0.10f, 1.0f));
        if (ImGui::Button("Stop##pc", ImVec2(stopW, 0)))
            host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
                [&host] { host.StopPlayMode(); }, [] {}, false));
        ImGui::PopStyleColor(3);
        ImGui::EndDisabled();
    }

    // ── Right side: Scene | API | ⚙ Settings ─────────────────────────────────
    {
        const float pad = ImGui::GetStyle().FramePadding.x;

        // Build API label
        static const struct { const char* name; CHEngine::ERenderAPI api; } kApis[] = {
            { "OpenGL", CHEngine::ERenderAPI::OPENGL },
            { "Vulkan",  CHEngine::ERenderAPI::VULKAN },
            { "Metal",   CHEngine::ERenderAPI::METAL  },
        };
        CHEngine::ERenderAPI curApi = CHEngine::Application::Get().GetRenderAPIType();
        const char* apiName = "?";
        for (auto& e : kApis) if (e.api == curApi) { apiName = e.name; break; }
        char apiLabel[32]; snprintf(apiLabel, sizeof(apiLabel), "API: %s", apiName);

        const float sceneW    = ImGui::CalcTextSize("Scene \xe2\x96\xbe").x + pad * 2.0f + 6.0f; // "Scene ▾"
        const float apiW      = ImGui::CalcTextSize(apiLabel).x + pad * 2.0f + 6.0f;
        const float settingsW = ImGui::CalcTextSize("\xe2\x9a\x99").x + pad * 2.0f + 10.0f;       // "⚙"
        const float rightBlock = sceneW + 4.0f + apiW + 4.0f + settingsW + ImGui::GetStyle().WindowPadding.x;

        float rx = ImGui::GetWindowWidth() - rightBlock;
        if (rx > ImGui::GetCursorPosX() + 10.0f)
        {
            ImGui::SetCursorPosX(rx);

            // Scene ▾
            vcenter(ImGui::GetFrameHeight());
            if (ImGui::Button("Scene \xe2\x96\xbe##scenemenu", ImVec2(sceneW, 0)))
                ImGui::OpenPopup("##scene_dropdown");

            if (ImGui::BeginPopup("##scene_dropdown"))
            {
                if (ImGui::MenuItem("Save Scene"))
                    host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
                        [&host] { host.SaveScene(); }, [] {}, false));
                if (ImGui::MenuItem("Open Scene..."))
                    host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
                        [&host] { host.ToggleSceneBrowser(); }, [] {}, false));
                ImGui::Separator();
                if (ImGui::MenuItem("+ New Session"))
                    host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
                        [&host] { host.AddSceneSession(); }, [] {}, false));
                ImGui::EndPopup();
            }

            // API
            ImGui::SameLine(0, 4);
            vcenter(ImGui::GetFrameHeight());
            if (ImGui::Button(apiLabel, ImVec2(apiW, 0)))
                ImGui::OpenPopup("##api_dropdown");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Switch render API (restarts engine)");

            if (ImGui::BeginPopup("##api_dropdown"))
            {
                for (auto& e : kApis)
                {
                    if (!CHEngine::RenderModuleResolver::IsSupportedOnPlatform(e.api)) continue;
                    bool sel = (e.api == curApi);
                    if (ImGui::MenuItem(e.name, nullptr, sel) && !sel)
                    {
                        const CHEngine::ERenderAPI chosen = e.api;
                        host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
                            [&host, chosen] { host.OnRendererApiSelected(chosen); }, [] {}, false));
                    }
                }
                ImGui::EndPopup();
            }

            // ⚙ Settings
            ImGui::SameLine(0, 4);
            vcenter(ImGui::GetFrameHeight());
            if (ImGui::Button("\xe2\x9a\x99##settings", ImVec2(settingsW, 0)))
                ImGui::OpenPopup("##settings_popup");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Settings");

            if (ImGui::BeginPopup("##settings_popup"))
            {
                UIActive::Toggle("Grid",       &host.GetEditorViewport().ShowGrid());
                UIActive::Toggle("Profiler",   &host.GetShowProfiler());
                UIActive::Toggle("UV Editor",  &host.GetShowUVEditor());
                ImGui::Separator();
                const char* themeLabel = (UIActive::g_Theme == AppTheme::RetroOS) ? "Theme: Retro" : "Theme: Dark";
                if (ImGui::MenuItem(themeLabel))
                    host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
                        [&host] { host.ToggleUiTheme(); }, [] {}, false));
                ImGui::EndPopup();
            }
        }
    }

    UIActive::EndToolbar();
}

} // namespace Sandbox
