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

void ToolbarPanel::Draw(EditorUiHost& host, ImVec2 pos, ImVec2 size)
{
    UIActive::BeginToolbar(pos, size);
    SceneSession& activeSession = host.GetActiveSceneSession();

    {
        const float winH = ImGui::GetWindowHeight();
        const float padY = 9.0f;
        const float startY = ImGui::GetCursorPosY();
        const float centerY = startY + (winH - padY * 2.0f) * 0.5f;

        struct VC {
            float cy;
            void operator()(float itemH) const { ImGui::SetCursorPosY(cy - itemH * 0.5f); }
        } vcenter{ centerY };

        if (!ImGui::GetIO().WantTextInput)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_T, false))
            {
                host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
                    [&host] { host.GetGizmoOperation() = ImGuizmo::TRANSLATE; }, [] {}, false));
            }
            if (ImGui::IsKeyPressed(ImGuiKey_R, false))
            {
                host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
                    [&host] { host.GetGizmoOperation() = ImGuizmo::ROTATE; }, [] {}, false));
            }
            if (ImGui::IsKeyPressed(ImGuiKey_S, false))
            {
                host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
                    [&host] { host.GetGizmoOperation() = ImGuizmo::SCALE; }, [] {}, false));
            }
            if (ImGui::IsKeyPressed(ImGuiKey_F3, false))
            {
                host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
                    [&host] { host.GetShowProfiler() = !host.GetShowProfiler(); }, [] {}, false));
            }

            const bool undoMod = ImGui::GetIO().KeySuper || ImGui::GetIO().KeyAlt;
            if (undoMod && ImGui::IsKeyPressed(ImGuiKey_Z, false))
            {
                host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
                    [&host] { host.RequestUndo(); }, [] {}, false));
            }
        }

        const char* gizmoLabels[] = { "Translate", "Rotate", "Scale" };
        int gizmoSel = (host.GetGizmoOperation() == ImGuizmo::TRANSLATE)   ? 0
 : (host.GetGizmoOperation() == ImGuizmo::ROTATE) ? 1
 : 2;

        vcenter(ImGui::GetFrameHeight());
        if (UIActive::SegmentedControl("##gizmo", gizmoLabels, 3, &gizmoSel, ImVec2(272.0f, 0.0f)))
        {
            host.GetGizmoOperation() = (gizmoSel == 0) ? ImGuizmo::TRANSLATE : (gizmoSel == 1) ? ImGuizmo::ROTATE : ImGuizmo::SCALE;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("T  Translate\nR  Rotate\nS  Scale");

        ImGui::SameLine(0, 20);
        vcenter(20.0f);
        if (UIActive::Toggle("Local", &host.GetLocalMode()))
            host.GetGizmoMode() = host.GetLocalMode() ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

        ImGui::SameLine(0, 20);
        vcenter(20.0f);
        UIActive::Toggle("Grid", &host.GetEditorViewport().ShowGrid());

        ImGui::SameLine(0, 12);
        vcenter(20.0f);
        UIActive::Toggle("Profiler", &host.GetShowProfiler());

        {
            if (!ImGui::GetIO().WantTextInput)
            {
                const bool mod = ImGui::GetIO().KeySuper || ImGui::GetIO().KeyCtrl;
                if (mod && ImGui::IsKeyPressed(ImGuiKey_P, false))
                {
                    host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
                        [&host] {
                            SceneSession& s = host.GetActiveSceneSession();
                            if (s.SessionState == SceneSession::State::Edit)
                                host.EnterPlayMode();
                            else if (s.SessionState == SceneSession::State::Play)
                                host.EnterPauseMode();
                            else if (s.SessionState == SceneSession::State::Pause)
                                host.ResumeFromPause();
                        },
                        [] {}, false));
                }
                if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)
                    && host.GetActiveSceneSession().SessionState != SceneSession::State::Edit)
                {
                    host.GetCommandStack().Push(
                        CHEngine::MakeScope<CallbackCommand>([&host] { host.StopPlayMode(); }, [] {}, false));
                }
            }

            const bool isEdit = (activeSession.SessionState == SceneSession::State::Edit);
            const bool isPlay = (activeSession.SessionState == SceneSession::State::Play);
            const bool isPause = (activeSession.SessionState == SceneSession::State::Pause);

            const float pad = ImGui::GetStyle().FramePadding.x;
            const float btnW = ImGui::CalcTextSize("Pause").x + pad * 2.0f + 14.0f;
            const float stopW = ImGui::CalcTextSize("Stop").x + pad * 2.0f + 14.0f;
            const float stepW = ImGui::CalcTextSize("Step").x + pad * 2.0f + 10.0f;
            const float gap = 4.0f;
            const float blockW = btnW + stopW + stepW + gap * 2.0f;

            ImVec2 winPos = ImGui::GetWindowPos();
            float winW = ImGui::GetWindowWidth();
            float winH2 = ImGui::GetWindowHeight();
            float screenX = winPos.x + (winW - blockW) * 0.5f;
            float screenY = winPos.y + (winH2 - ImGui::GetFrameHeight()) * 0.5f;
            ImGui::SetCursorScreenPos(ImVec2(screenX, screenY));

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.50f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.62f, 0.20f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.10f, 0.38f, 0.10f, 1.0f));
            if (isPlay)
            {
                if (ImGui::Button("Pause##playctrl", ImVec2(btnW, 0)))
                {
                    host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
                        [&host] { host.EnterPauseMode(); }, [] {}, false));
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Pause  (Cmd+P)");
            }
            else
            {
                const char* label = isPause ? "Resume##playctrl" : "Play##playctrl";
                if (ImGui::Button(label, ImVec2(btnW, 0)))
                {
                    host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
                        [&host, isEdit] {
                            if (isEdit)
                                host.EnterPlayMode();
                            else
                                host.ResumeFromPause();
                        },
                        [] {}, false));
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip(isEdit ? "Play  (Cmd+P)" : "Resume  (Cmd+P)");
            }
            ImGui::PopStyleColor(3);

            ImGui::SameLine(0, gap);
            vcenter(ImGui::GetFrameHeight());

            ImGui::BeginDisabled(isEdit);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.50f, 0.15f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.62f, 0.20f, 0.20f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.38f, 0.10f, 0.10f, 1.0f));
            if (ImGui::Button("Stop##playctrl", ImVec2(stopW, 0)))
            {
                host.GetCommandStack().Push(
                    CHEngine::MakeScope<CallbackCommand>([&host] { host.StopPlayMode(); }, [] {}, false));
            }
            ImGui::PopStyleColor(3);
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Stop  (Esc)");

            ImGui::SameLine(0, gap);
            vcenter(ImGui::GetFrameHeight());

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Step one frame  (Cmd+Shift+Right)");
        }

        {
            ImGui::SameLine(0, 12);
            vcenter(ImGui::GetFrameHeight());
            ImGui::BeginDisabled(host.GetActiveSessionIndex() == 0);
            if (ImGui::ArrowButton("##session_prev", ImGuiDir_Left))
            {
                host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
                    [&host] { host.SetActiveSessionIndex(host.GetActiveSessionIndex() - 1); }, [] {}, false));
            }
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Previous scene tab");

            ImGui::SameLine(0, 4);
            vcenter(ImGui::GetTextLineHeight());
            {
                const auto& sessions = host.GetSceneSessions();
                const size_t idx = host.GetActiveSessionIndex();
                const std::string name = (idx < sessions.size()) ? sessions[idx].DisplayName() : std::string("?");
                ImGui::Text("%s  %u/%u",
                            name.c_str(),
                            static_cast<uint32_t>(idx + 1),
                            static_cast<uint32_t>(sessions.size()));
            }

            ImGui::SameLine(0, 4);
            vcenter(ImGui::GetFrameHeight());
            ImGui::BeginDisabled(host.GetActiveSessionIndex() + 1 >= host.GetSceneSessions().size());
            if (ImGui::ArrowButton("##session_next", ImGuiDir_Right))
            {
                host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
                    [&host] { host.SetActiveSessionIndex(host.GetActiveSessionIndex() + 1); }, [] {}, false));
            }
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Next scene tab");

            ImGui::SameLine(0, 6);
            vcenter(ImGui::GetFrameHeight());
            ImGui::BeginDisabled(host.GetSceneSessions().size() <= 1);
            if (ImGui::Button("x##close_session"))
            {
                // Call directly — NOT via CommandStack, because CloseSceneSession
                // erases the session that owns the active CommandStack (use-after-free).
                host.CloseSceneSession(host.GetActiveSessionIndex());
            }
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Close current scene tab");

            ImGui::SameLine(0, 4);
            vcenter(ImGui::GetFrameHeight());
            if (ImGui::Button("+ Scene"))
            {
                host.GetCommandStack().Push(
                    CHEngine::MakeScope<CallbackCommand>([&host] { host.AddSceneSession(); }, [] {}, false));
            }
        }

        {
            bool shiftHeld = ImGui::GetIO().KeyShift;
            ImVec4 col = shiftHeld ? ImVec4(0.20f, 0.60f, 1.00f, 1.0f) : ImVec4(0.43f, 0.43f, 0.45f, 1.0f);
            ImGui::SameLine(0, 20);
            vcenter(ImGui::GetTextLineHeight());
            ImGui::PushStyleColor(ImGuiCol_Text, col);
            ImGui::TextUnformatted(shiftHeld ? "\xe2\x87\xa7 Snap ON" : "\xe2\x87\xa7 Snap");
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Hold Shift to snap:\n"
                                  "  Translate  1 unit\n"
                                  "  Rotate     45\xc2\xb0\n"
                                  "  Scale      0.1 unit");
        }

        float fps = ImGui::GetIO().Framerate;
        char fpsBuf[32];
        snprintf(fpsBuf, sizeof(fpsBuf), "%.0f fps", fps);
        const char* themeLabel = (UIActive::g_Theme == AppTheme::RetroOS) ? "Theme: Retro" : "Theme: Dark";
        const float pad = ImGui::GetStyle().FramePadding.x;

        struct ApiEntry {
            const char* name;
            CHEngine::ERenderAPI api;
        };
        static const ApiEntry allApis[] = {
            { "OpenGL", CHEngine::ERenderAPI::OPENGL },
            { "Vulkan", CHEngine::ERenderAPI::VULKAN },
            { "Metal", CHEngine::ERenderAPI::METAL },
        };

        ApiEntry availApis[3];
        int availCount = 0;
        for (auto& e : allApis)
        {
            if (CHEngine::RenderModuleResolver::IsSupportedOnPlatform(e.api))
                availApis[availCount++] = e;
        }

        CHEngine::ERenderAPI curApi = CHEngine::Application::Get().GetRenderAPIType();
        int apiIdx = 0;
        for (int i = 0; i < availCount; ++i)
        {
            if (availApis[i].api == curApi)
            {
                apiIdx = i;
                break;
            }
        }

        char rendererLabel[32];
        snprintf(rendererLabel, sizeof(rendererLabel), "API: %s", availApis[apiIdx].name);
        float rendererComboW = ImGui::CalcTextSize(rendererLabel).x + pad * 2.0f + 8.0f;

        // Project info / switcher
        {
            static char s_NameBuf[256] = "MyProject";
            static std::string s_ParentDir;
            static bool s_AwaitingName = false;
            static bool s_OpenNamePopup = false;

            const char* projLabel = CHEngine::ProjectManager::HasProject()
                ? CHEngine::ProjectManager::Current()->GetName().c_str()
                : "No Project";

            ImGui::SameLine(0, 16);
            vcenter(ImGui::GetFrameHeight());
            if (ImGui::Button(projLabel))
                ImGui::OpenPopup("##ProjectMenu");

            if (ImGui::BeginPopup("##ProjectMenu"))
            {
                if (ImGui::MenuItem("New Project..."))
                {
                    std::string folder = CHEngine::FileDialog::SelectFolder("New Project: Select parent folder");
                    if (!folder.empty())
                    {
                        s_ParentDir = folder;
                        std::snprintf(s_NameBuf, sizeof(s_NameBuf), "MyProject");
                        s_AwaitingName = true;
                        s_OpenNamePopup = true;
                    }
                }
                if (ImGui::MenuItem("Open Project..."))
                {
                    const char* filters[] = { "*.cheproj" };
                    std::string path = CHEngine::FileDialog::OpenFile(
                        "CHEngine Project (*.cheproj)", filters, 1, "Open Project");
                    if (!path.empty() && CHEngine::ProjectManager::Open(path))
                    {
                        CHEngine::EngineConfig::SaveLastProject(path);
                        CHEngine::EngineConfig::AddRecentProject(path);
                        host.GetCommandStack().Push(
                            CHEngine::MakeScope<CallbackCommand>(
                                [&host] { host.OnProjectChanged(); }, [] {}, false));
                    }
                }

                const auto recents = CHEngine::EngineConfig::LoadRecentProjects();
                if (!recents.empty())
                {
                    ImGui::Separator();
                    ImGui::TextDisabled("Recent");
                    for (const auto& rp : recents)
                    {
                        const std::string stem = std::filesystem::path(rp).stem().string();
                        if (ImGui::MenuItem(stem.c_str()))
                        {
                            if (CHEngine::ProjectManager::Open(rp))
                            {
                                CHEngine::EngineConfig::SaveLastProject(rp);
                                CHEngine::EngineConfig::AddRecentProject(rp);
                                host.GetCommandStack().Push(
                                    CHEngine::MakeScope<CallbackCommand>(
                                        [&host] { host.OnProjectChanged(); }, [] {}, false));
                            }
                        }
                    }
                }
                ImGui::EndPopup();
            }

            // New-project name popup
            if (s_AwaitingName)
            {
                if (s_OpenNamePopup)
                {
                    ImGui::OpenPopup("##NewProjNameTB");
                    s_OpenNamePopup = false;
                }
                const ImGuiIO& io2 = ImGui::GetIO();
                ImGui::SetNextWindowPos(
                    ImVec2(io2.DisplaySize.x * 0.5f, io2.DisplaySize.y * 0.5f),
                    ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                if (ImGui::BeginPopupModal("##NewProjNameTB", nullptr,
                                           ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar))
                {
                    ImGui::Text("Project name:");
                    ImGui::SetNextItemWidth(280.0f);
                    bool confirm = ImGui::InputText("##pname", s_NameBuf, sizeof(s_NameBuf),
                                                    ImGuiInputTextFlags_EnterReturnsTrue);
                    ImGui::SameLine();
                    confirm |= ImGui::Button("Create");
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel"))
                    {
                        s_AwaitingName = false;
                        ImGui::CloseCurrentPopup();
                    }
                    if (confirm && s_NameBuf[0] != '\0')
                    {
                        std::string path = CHEngine::ProjectManager::Create(s_ParentDir, s_NameBuf);
                        if (!path.empty())
                        {
                            CHEngine::EngineConfig::SaveLastProject(path);
                            CHEngine::EngineConfig::AddRecentProject(path);
                            host.GetCommandStack().Push(
                                CHEngine::MakeScope<CallbackCommand>(
                                    [&host] { host.OnProjectChanged(); }, [] {}, false));
                        }
                        s_AwaitingName = false;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }
            }
        }

        float saveSceneW = ImGui::CalcTextSize("Save Scene").x + pad * 2.0f;
        float openSceneW = ImGui::CalcTextSize("Open Scene").x + pad * 2.0f;

        float rightBlockW = saveSceneW + 4.0f + openSceneW + 12.0f + ImGui::CalcTextSize("|").x + 12.0f
 + rendererComboW + 8.0f + ImGui::CalcTextSize(themeLabel).x + pad * 2.0f + 8.0f
            + ImGui::CalcTextSize(fpsBuf).x + ImGui::GetStyle().WindowPadding.x;

        float startX = ImGui::GetWindowWidth() - rightBlockW;
        if (startX > ImGui::GetCursorPosX() + 10.0f)
        {
            ImGui::SetCursorPosX(startX);

            vcenter(ImGui::GetFrameHeight());
            if (ImGui::Button("Save Scene"))
            {
                host.GetCommandStack().Push(
                    CHEngine::MakeScope<CallbackCommand>([&host] { host.SaveScene(); }, [] {}, false));
            }
            ImGui::SameLine(0, 4);
            vcenter(ImGui::GetFrameHeight());
            if (ImGui::Button("Open Scene"))
            {
                host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
                    [&host] { host.ToggleSceneBrowser(); }, [] {}, false));
            }

            ImGui::SameLine(0, 12);
            vcenter(20.0f);
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine(0, 12);

            vcenter(ImGui::GetFrameHeight());
            ImGui::SetNextItemWidth(rendererComboW);
            if (ImGui::BeginCombo("##renderer", rendererLabel, ImGuiComboFlags_NoArrowButton))
            {
                for (int i = 0; i < availCount; ++i)
                {
                    bool selected = (i == apiIdx);
                    if (ImGui::Selectable(availApis[i].name, selected))
                    {
                        if (i != apiIdx)
                        {
                            const CHEngine::ERenderAPI chosen = availApis[i].api;
                            host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
                                [&host, chosen] { host.OnRendererApiSelected(chosen); }, [] {}, false));
                        }
                    }
                }
                ImGui::EndCombo();
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Switch render API (restarts engine)");

            ImGui::SameLine(0, 8);
            vcenter(ImGui::GetFrameHeight());
            if (ImGui::Button(themeLabel))
            {
                host.GetCommandStack().Push(
                    CHEngine::MakeScope<CallbackCommand>([&host] { host.ToggleUiTheme(); }, [] {}, false));
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Switch UI theme");
            ImGui::SameLine(0, 8);
            vcenter(ImGui::GetTextLineHeight());
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
            ImGui::TextUnformatted(fpsBuf);
            ImGui::PopStyleColor();
        }
    }

    UIActive::EndToolbar();
}

} // namespace Sandbox
