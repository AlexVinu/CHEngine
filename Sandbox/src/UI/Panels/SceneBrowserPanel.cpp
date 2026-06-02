#include "SceneBrowserPanel.h"

#include "EditorContext.h"
#include "SceneViewLayer_IO.h"
#include "ProjectManager.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace Sandbox {

namespace {

struct SceneEntry
{
    std::string rel;       // generic, e.g. "Scenes/Main.chscene"
    std::string display;   // stem, e.g. "Main"
    bool isStartup = false;
};

std::vector<SceneEntry> CollectScenes(const Project& proj)
{
    std::vector<SceneEntry> out;
    const fs::path scenesDir = proj.ScenesAbsPath();
    std::error_code ec;
    if (!fs::exists(scenesDir, ec))
        return out;

    const std::string startup = proj.GetStartupScene();

    for (auto it = fs::recursive_directory_iterator(scenesDir, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec))
    {
        if (ec) break;
        if (!it->is_regular_file(ec)) continue;
        if (it->path().extension() != ".chscene") continue;

        SceneEntry e;
        e.rel = fs::relative(it->path(), proj.RootDir(), ec).generic_string();
        e.display = it->path().stem().string();
        e.isStartup = (e.rel == startup);
        out.push_back(std::move(e));
    }

    std::sort(out.begin(), out.end(),
              [](const SceneEntry& a, const SceneEntry& b) { return a.rel < b.rel; });
    return out;
}

} // namespace

void SceneBrowserPanel::OnImGuiRender(EditorContext& ctx)
{
    if (!m_Open)
        return;

    Ref<ProjectManager> proj_manager = ctx.Projects;
    if (!proj_manager || !proj_manager->HasProject())
    {
        m_Open = false;
        return;
    }
    Project* proj = proj_manager->Current();

    ImGui::SetNextWindowSize(ImVec2(460, 360), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Scene Browser", &m_Open))
    {
        ImGui::End();
        return;
    }

    ImGui::Text("Project: %s", proj->GetName().c_str());
    ImGui::Separator();

    if (ImGui::Button("+ New Scene"))
    {
        SceneViewLayerIO::NewSceneFile(ctx);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(double-click an entry to open)");

    ImGui::Separator();

    const auto scenes = CollectScenes(*proj);

    if (scenes.empty())
        ImGui::TextDisabled("No scenes in this project yet.");

    for (const SceneEntry& e : scenes)
    {
        ImGui::PushID(e.rel.c_str());

        const bool renaming = (m_RenameTarget == e.rel);
        if (renaming)
        {
            ImGui::SetNextItemWidth(220.0f);
            if (ImGui::InputText("##rename", m_RenameBuf, sizeof(m_RenameBuf),
                                 ImGuiInputTextFlags_EnterReturnsTrue))
            {
                SceneViewLayerIO::RenameSceneFile(ctx, e.rel, m_RenameBuf);
                m_RenameTarget.clear();
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Cancel"))
                m_RenameTarget.clear();
        }
        else
        {
            std::string label = e.display;
            if (e.isStartup)
                label = "* " + label + "  (startup)";

            const bool clicked = ImGui::Selectable(label.c_str(), false,
                                                   ImGuiSelectableFlags_AllowDoubleClick,
                                                   ImVec2(220, 0));
            if (clicked && ImGui::IsMouseDoubleClicked(0))
                SceneViewLayerIO::OpenSceneFile(ctx, e.rel);
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("Open"))
            SceneViewLayerIO::OpenSceneFile(ctx, e.rel);
        ImGui::SameLine();
        if (ImGui::SmallButton("Startup"))
            SceneViewLayerIO::SetStartupSceneFile(ctx, e.rel);
        ImGui::SameLine();
        if (ImGui::SmallButton("Rename"))
        {
            m_RenameTarget = e.rel;
            std::snprintf(m_RenameBuf, sizeof(m_RenameBuf), "%s", e.display.c_str());
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Delete"))
            m_PendingDelete = e.rel;

        ImGui::PopID();
    }

    // Delete confirm popup.
    if (!m_PendingDelete.empty())
        ImGui::OpenPopup("Confirm Delete");

    if (ImGui::BeginPopupModal("Confirm Delete", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Delete '%s'?\nThis cannot be undone.", m_PendingDelete.c_str());
        ImGui::Separator();
        if (ImGui::Button("Delete", ImVec2(120, 0)))
        {
            SceneViewLayerIO::DeleteSceneFile(ctx, m_PendingDelete);
            m_PendingDelete.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            m_PendingDelete.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}

} // namespace Sandbox
