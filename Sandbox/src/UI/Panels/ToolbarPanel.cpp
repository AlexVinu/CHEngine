#include "ToolbarPanel.h"

#include "EditorViewport.h"
#include "SetTransformCommand.h"

#include <CHEngine/Application.h>
#include <CHEngine/EngineConfig.h>
#include <CHEngine/Utils/FileDialog.h>

#include "InputActions.h"
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
    m_IconPlay      = CHEngine::RenderFacade::CreateTextureFromFile("editor_assets/icons/icon_play.png");
    m_IconPause     = CHEngine::RenderFacade::CreateTextureFromFile("editor_assets/icons/icon_pause.png");
    m_IconStop      = CHEngine::RenderFacade::CreateTextureFromFile("editor_assets/icons/icon_stop.png");
    m_IconResume    = CHEngine::RenderFacade::CreateTextureFromFile("editor_assets/icons/icon_resume.png");
    m_IconsLoaded   = true;
}

// Возвращает ImTextureID из TextureHandle (работает для Metal и OGL)
static ImTextureID ToImTex(CHEngine::TextureHandle h)
{
    if (!h.IsValid()) return static_cast<ImTextureID>(0);
    auto* f = CHEngine::RenderFacade::GetRenderFactory();
    if (!f) return static_cast<ImTextureID>(0);
    return static_cast<ImTextureID>(f->GetTextureNativeID(h));
}

void ToolbarPanel::Draw(SceneViewLayerHost& host, ImVec2 pos, ImVec2 size)
{
    // Ленивая загрузка: при первом Draw factory точно уже готова
    if (!m_IconsLoaded)
        LoadIcons();
    UIActive::BeginToolbar(pos, size);
    Ref<EditorWorldContext> activeSession = host.GetActiveSceneSession();

    const float winH = ImGui::GetWindowHeight();
    const float startY = ImGui::GetCursorPosY();
    const float centerY = startY + (winH - 18.0f) * 0.5f;

    struct VC {
        float cy;
        void operator()(float itemH) const { ImGui::SetCursorPosY(cy - itemH * 0.5f); }
    } vcenter{ centerY };

    // ── Keyboard shortcuts (driven by InputActions / keybindings.json) ───────
    if (!ImGui::GetIO().WantTextInput)
    {
        using IA = Sandbox::InputActions;

        if (IA::Triggered("Editor.Gizmo.Translate"))
            host.GetCommandStack().Push(MakeScope<CallbackCommand>(
                [&host] { host.GetGizmoOperation() = ImGuizmo::TRANSLATE; }, [] {}, false));
        if (IA::Triggered("Editor.Gizmo.Rotate"))
            host.GetCommandStack().Push(MakeScope<CallbackCommand>(
                [&host] { host.GetGizmoOperation() = ImGuizmo::ROTATE; }, [] {}, false));
        if (IA::Triggered("Editor.Gizmo.Scale"))
            host.GetCommandStack().Push(MakeScope<CallbackCommand>(
                [&host] { host.GetGizmoOperation() = ImGuizmo::SCALE; }, [] {}, false));
        if (IA::Triggered("Editor.Profiler.Toggle"))
            host.GetShowProfiler() = !host.GetShowProfiler();

        // Delete / Backspace — удаление выделенного объекта (только в Edit-режиме)
        if (IA::Triggered("Editor.Entity.Delete")
            && activeSession->GetSessionState() == SceneSession::State::Edit)
        {
            Ref<EditorWorldContext> s = host.GetActiveSceneSession();
            if (s->EditorScene && s->EditorScene->IsEntityHandleValid(s->SelectedEntity))
            {
                const CHEngine::UUID id = s->EditorScene->GetUUID(s->SelectedEntity);
                host.GetCommandStack().Push(MakeScope<CallbackCommand>(
                    [&host, id] { host.DestroyEntityByUuid(id); }, [] {}, false));
            }
        }

        if (IA::Triggered("Editor.History.Undo"))
            host.GetCommandStack().Push(MakeScope<CallbackCommand>(
                [&host] { host.RequestUndo(); }, [] {}, false));

        if (IA::Triggered("Editor.Play.Toggle"))
        {
            host.GetCommandStack().Push(MakeScope<CallbackCommand>(
                [&host] {
                    Ref<EditorWorldContext> s = host.GetActiveSceneSession();
                    if (s->GetSessionState() == SceneSession::State::Edit)        host.EnterPlayMode();
                    else if (s->GetSessionState() == SceneSession::State::Play)   host.EnterPauseMode();
                    else if (s->GetSessionState() == SceneSession::State::Pause)  host.ResumeFromPause();
                }, [] {}, false));
        }
        if (IA::Triggered("Editor.Play.Stop") &&
            activeSession->GetSessionState() != SceneSession::State::Edit)
        {
            host.GetCommandStack().Push(MakeScope<CallbackCommand>(
                [&host] { host.StopPlayMode(); }, [] {}, false));
        }
    }

    // ── Session navigation (left side) ────────────────────────────────────────
    {
        vcenter(ImGui::GetFrameHeight());
        ImGui::BeginDisabled(host.GetActiveSessionIndex() == 0);
        if (ImGui::ArrowButton("##prev", ImGuiDir_Left))
            host.GetCommandStack().Push(MakeScope<CallbackCommand>(
                [&host] { host.SetActiveSessionIndex(host.GetActiveSessionIndex() - 1); }, [] {}, false));
        ImGui::EndDisabled();

        ImGui::SameLine(0, 4);
        vcenter(ImGui::GetTextLineHeight());
        {
            const auto sessions = host.GetSceneSessions();
            const size_t idx = host.GetActiveSessionIndex();
            const std::string name = (sessions && idx < sessions->size()) ? (*sessions)[idx]->DisplayName() : "?";
            ImGui::Text("%s  %u/%u", name.c_str(),
                        static_cast<uint32_t>(idx + 1),
                        static_cast<uint32_t>(sessions ? sessions->size() : 0));
        }

        ImGui::SameLine(0, 4);
        vcenter(ImGui::GetFrameHeight());
        ImGui::BeginDisabled(host.GetActiveSessionIndex() + 1 >= (host.GetSceneSessions() ? host.GetSceneSessions()->size() : 0));
        if (ImGui::ArrowButton("##next", ImGuiDir_Right))
            host.GetCommandStack().Push(MakeScope<CallbackCommand>(
                [&host] { host.SetActiveSessionIndex(host.GetActiveSessionIndex() + 1); }, [] {}, false));
        ImGui::EndDisabled();

        ImGui::SameLine(0, 6);
        vcenter(ImGui::GetFrameHeight());
        ImGui::BeginDisabled(!host.GetSceneSessions() || host.GetSceneSessions()->size() <= 1);
        if (ImGui::Button("x##close"))
            host.CloseSceneSession(host.GetActiveSessionIndex());
        ImGui::EndDisabled();

        ImGui::SameLine(0, 4);
        vcenter(ImGui::GetFrameHeight());
        if (ImGui::Button("+ Scene"))
        {
            host.GetCommandStack().Push(
                MakeScope<CallbackCommand>([&host] { host.AddSceneSession(); }, [] {}, false));
        }
    }

    // ── Play / Stop (centered) ────────────────────────────────────────────────
    {
        const bool isEdit  = (activeSession->GetSessionState() == SceneSession::State::Edit);
        const bool isPlay  = (activeSession->GetSessionState() == SceneSession::State::Play);
        const bool isPause = (activeSession->GetSessionState() == SceneSession::State::Pause);

        const float kBtnH    = winH * 0.60f;       // высота кнопки (чуть меньше)
        const float kPadX    = 10.0f;              // горизонтальный отступ
        const float kIcoH    = kBtnH - 6.0f;       // размер иконки (квадрат)
        const float kTextGap = 5.0f;               // отступ между текстом и иконкой
        const float kRound   = 4.0f;               // скругление углов
        const float gap      = 5.0f;               // зазор между двумя кнопками

        const bool   isMetal = (CHEngine::Application::Get().GetRenderAPIType() == CHEngine::ERenderAPI::METAL);
        const ImVec2 uv0     = isMetal ? ImVec2(0,0) : ImVec2(0,1);
        const ImVec2 uv1     = isMetal ? ImVec2(1,1) : ImVec2(1,0);

        // Хелпер: рисует кнопку "Текст + отступ + Иконка" через InvisibleButton + DrawList
        auto playBtn = [&](const char* strId, const char* label,
                           CHEngine::TextureHandle icon,
                           ImVec4 colNorm, ImVec4 colHov, ImVec4 colAct,
                           bool disabled) -> bool
        {
            ImTextureID tex    = ToImTex(icon);
            float textW        = ImGui::CalcTextSize(label).x;
            float contentW     = textW + (tex ? kTextGap + kIcoH : 0.0f);
            float btnW         = contentW + kPadX * 2.0f;

            if (disabled) ImGui::BeginDisabled(true);

            ImVec2 pos = ImGui::GetCursorScreenPos();
            bool clicked = ImGui::InvisibleButton(strId, ImVec2(btnW, kBtnH));

            // Цвет фона по состоянию
            ImVec4 col4 = ImGui::IsItemActive()  ? colAct
                        : ImGui::IsItemHovered() ? colHov
                        : colNorm;
            if (disabled) col4.w *= 0.4f;

            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(pos, ImVec2(pos.x + btnW, pos.y + kBtnH),
                              ImGui::ColorConvertFloat4ToU32(col4), kRound);

            // Контент (текст + иконка) центрируем горизонтально и вертикально
            float contentX = pos.x + (btnW - contentW) * 0.5f;

            // Текст — центр по вертикали
            float textY = pos.y + (kBtnH - ImGui::GetTextLineHeight()) * 0.5f;
            ImU32 textCol = disabled ? IM_COL32(200,200,200,100) : IM_COL32(255,255,255,255);
            dl->AddText(ImVec2(contentX, textY), textCol, label);

            // Иконка — центр по вертикали, сразу после текста
            if (tex)
            {
                float ix = contentX + textW + kTextGap;
                float iy = pos.y + (kBtnH - kIcoH) * 0.5f;
                ImU32 tintCol = disabled ? IM_COL32(255,255,255,100) : IM_COL32(255,255,255,255);
                dl->AddImage(tex, ImVec2(ix, iy), ImVec2(ix + kIcoH, iy + kIcoH),
                             uv0, uv1, tintCol);
            }

            if (disabled) ImGui::EndDisabled();
            return clicked && !disabled;
        };

        // Сначала вычисляем суммарную ширину блока для центровки
        const char* playLabel = isPlay ? "Pause" : (isPause ? "Resume" : "Play");
        CHEngine::TextureHandle playIcon = isPlay  ? m_IconPause    // играет  → кнопка "Pause"
                                         : isPause ? m_IconResume   // пауза   → кнопка "Resume"
                                         : m_IconPlay;              // редактор → кнопка "Play"
        float playTextW  = ImGui::CalcTextSize(playLabel).x;
        float playIconW  = ToImTex(playIcon) ? kTextGap + kIcoH : 0.0f;
        float playBtnW   = playTextW + playIconW + kPadX * 2.0f;

        float stopTextW  = ImGui::CalcTextSize("Stop").x;
        float stopIconW  = ToImTex(m_IconStop) ? kTextGap + kIcoH : 0.0f;
        float stopBtnW   = stopTextW + stopIconW + kPadX * 2.0f;

        float blockW = playBtnW + stopBtnW + gap;
        float screenX = ImGui::GetWindowPos().x + (ImGui::GetWindowWidth() - blockW) * 0.5f;
        float screenY = ImGui::GetWindowPos().y + (winH - kBtnH) * 0.5f;
        ImGui::SetCursorScreenPos(ImVec2(screenX, screenY));

        // ── Play / Pause / Resume ────
        if (playBtn(isPlay ? "##btn_pause" : "##btn_play", playLabel, playIcon,
                    ImVec4(0.15f,0.50f,0.15f,1), ImVec4(0.20f,0.62f,0.20f,1),
                    ImVec4(0.10f,0.38f,0.10f,1), false))
            host.GetCommandStack().Push(MakeScope<CallbackCommand>(
                [&host, isPlay, isEdit] {
                    if (isPlay)      host.EnterPauseMode();
                    else if (isEdit) host.EnterPlayMode();
                    else             host.ResumeFromPause();
                }, [] {}, false));

        ImGui::SameLine(0, gap);

        // ── Stop ────
        if (playBtn("##btn_stop", "Stop", m_IconStop,
                    ImVec4(0.50f,0.15f,0.15f,1), ImVec4(0.62f,0.20f,0.20f,1),
                    ImVec4(0.38f,0.10f,0.10f,1), isEdit))
            host.GetCommandStack().Push(MakeScope<CallbackCommand>(
                [&host] { host.StopPlayMode(); }, [] {}, false));
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
                    host.GetCommandStack().Push(MakeScope<CallbackCommand>(
                        [&host] { host.SaveScene(); }, [] {}, false));
                if (ImGui::MenuItem("Open Scene..."))
                    host.GetCommandStack().Push(MakeScope<CallbackCommand>(
                        [&host] { host.ToggleSceneBrowser(); }, [] {}, false));
                ImGui::Separator();
                if (ImGui::MenuItem("+ New Session"))
                    host.GetCommandStack().Push(MakeScope<CallbackCommand>(
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
                        host.GetCommandStack().Push(MakeScope<CallbackCommand>(
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
                ImGui::Separator();
                const char* themeLabel = (UIActive::g_Theme == AppTheme::RetroOS) ? "Theme: Retro" : "Theme: Dark";
                if (ImGui::MenuItem(themeLabel))
                    host.GetCommandStack().Push(MakeScope<CallbackCommand>(
                        [&host] { host.ToggleUiTheme(); }, [] {}, false));
                ImGui::EndPopup();
            }
        }
    }

    UIActive::EndToolbar();
}

} // namespace Sandbox
