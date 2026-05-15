#include "ProjectBrowserWindow.h"
#include "ProjectManager.h"

#include <CHEngine/EngineConfig.h>
#include <CHEngine/Utils/FileDialog.h>

#include <imgui.h>
#include <Log/Log.h>
#include <filesystem>
#include <cstring>
#include <vector>

namespace fs = std::filesystem;

bool ProjectBrowserWindow::TryOpenProject(ProjectManager& proj_manager, const std::string& path)
{
    if (!proj_manager.Open(path))
        return false;
    CHEngine::EngineConfig::SaveLastProject(path);
    CHEngine::EngineConfig::AddRecentProject(path);
    return true;
}

bool ProjectBrowserWindow::StartNewProject()
{
    std::string folder = CHEngine::FileDialog::SelectFolder("New Project: Select parent folder");
    if (folder.empty())
        return false;
    m_ParentDir = folder;
    m_Step = Step::EnterName;
    m_OpenNamePopup = true;
    return false;
}

bool ProjectBrowserWindow::Draw(ProjectManager& proj_manager)
{
    bool projectOpened = false;

    const ImGuiIO& io = ImGui::GetIO();
    const float W = io.DisplaySize.x;
    const float H = io.DisplaySize.y;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(W, H));
    ImGui::SetNextWindowBgAlpha(1.0f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
                           | ImGuiWindowFlags_NoMove   | ImGuiWindowFlags_NoCollapse
                           | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin("##ProjectBrowser", nullptr, flags);

    // Center panel
    const float panelW = 560.0f;
    const float panelH = 460.0f;
    ImGui::SetCursorPos(ImVec2((W - panelW) * 0.5f, (H - panelH) * 0.5f));
    ImGui::BeginChild("##panel", ImVec2(panelW, panelH), true);

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
    ImGui::SetWindowFontScale(1.4f);
    ImGui::Text("CHEngine");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Buttons row
    if (ImGui::Button("New Project", ImVec2(130, 0)))
    {
        if (StartNewProject())
            projectOpened = true;
    }
    ImGui::SameLine(0, 8);
    if (ImGui::Button("Open Project", ImVec2(130, 0)))
    {
        const char* filters[] = { "*.cheproj" };
        std::string path = CHEngine::FileDialog::OpenFile("CHEngine Project (*.cheproj)", filters, 1, "Open Project");
        if (!path.empty())
            projectOpened = TryOpenProject(proj_manager, path);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextUnformatted("Recent Projects");
    ImGui::Spacing();

    std::vector<std::string> recents = CHEngine::EngineConfig::LoadRecentProjects();
    std::string clickedRecent = DrawRecentList(recents);
    if (!clickedRecent.empty())
        projectOpened = TryOpenProject(proj_manager, clickedRecent);

    if (recents.empty())
        ImGui::TextDisabled("  (no recent projects)");

    ImGui::EndChild();

    // "Enter project name" popup
    if (m_Step == Step::EnterName)
    {
        if (m_OpenNamePopup)
        {
            ImGui::OpenPopup("##NewProjName");
            m_OpenNamePopup = false;
        }
        ImGui::SetNextWindowPos(ImVec2(W * 0.5f, H * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("##NewProjName", nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar))
        {
            ImGui::Text("Project name:");
            ImGui::SetNextItemWidth(280.0f);
            bool confirm = ImGui::InputText("##name", m_NameBuf, sizeof(m_NameBuf),
                                            ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::SameLine();
            confirm |= ImGui::Button("Create");
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                m_Step = Step::Idle;
                ImGui::CloseCurrentPopup();
            }
            if (confirm && m_NameBuf[0] != '\0')
            {
                std::string path = proj_manager.Create(m_ParentDir, m_NameBuf);
                if (!path.empty())
                {
                    CHEngine::EngineConfig::SaveLastProject(path);
                    CHEngine::EngineConfig::AddRecentProject(path);
                    projectOpened = true;
                }
                m_Step = Step::Idle;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    ImGui::End();
    return projectOpened;
}

// Returns the path clicked, or empty string.
std::string ProjectBrowserWindow::DrawRecentList(const std::vector<std::string>& recents)
{
    std::string clicked;
    const float listH = 220.0f;
    ImGui::BeginChild("##recentList", ImVec2(0, listH), false);

    for (const auto& projPath : recents)
    {
        const fs::path p(projPath);
        const std::string displayName = p.stem().string();
        const std::string displayPath = p.parent_path().string();

        const bool exists = fs::exists(p);
        if (!exists)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));

        ImGui::PushID(projPath.c_str());
        if (ImGui::Selectable("##sel", false, ImGuiSelectableFlags_AllowOverlap, ImVec2(0, 36)))
        {
            if (exists)
                clicked = projPath;
        }
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 4.0f);
        ImGui::BeginGroup();
        ImGui::TextUnformatted(displayName.c_str());
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.55f, 1.0f));
        ImGui::TextUnformatted(displayPath.c_str());
        ImGui::PopStyleColor();
        ImGui::EndGroup();

        ImGui::PopID();
        if (!exists)
            ImGui::PopStyleColor();

        ImGui::Separator();
    }

    ImGui::EndChild();
    return clicked;
}
