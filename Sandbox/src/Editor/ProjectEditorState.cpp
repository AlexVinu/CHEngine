#include "ProjectEditorState.h"

#include <CHEngine/Project/Project.h>
#include <FileSystem/FileSystem.h>
#include <Log/Log.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace Sandbox {

static fs::path StateFilePath(const CHEngine::Project& proj)
{
    return proj.RootDir() / ".editor_state.json";
}

ProjectEditorState ProjectEditorState::LoadFor(const CHEngine::Project& proj)
{
    ProjectEditorState state;
    const fs::path path = StateFilePath(proj);
    if (!CHEngine::FileSystem::Exists(path))
        return state;

    const std::string text = CHEngine::FileSystem::ReadFileText(path);
    if (text.empty())
        return state;

    try {
        const nlohmann::json j = nlohmann::json::parse(text);
        if (j.contains("recent_scenes") && j["recent_scenes"].is_array())
        {
            for (const auto& s : j["recent_scenes"])
                if (s.is_string())
                    state.m_RecentScenes.push_back(s.get<std::string>());
        }
    } catch (const std::exception& e) {
        CHE_CORE_WARN("ProjectEditorState::LoadFor: parse error: {}", e.what());
    }
    return state;
}

void ProjectEditorState::Save(const CHEngine::Project& proj) const
{
    nlohmann::json j;
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& s : m_RecentScenes)
        arr.push_back(s);
    j["recent_scenes"] = arr;

    const fs::path path = StateFilePath(proj);
    if (!CHEngine::FileSystem::WriteFileText(path, j.dump(2) + "\n"))
        CHE_CORE_WARN("ProjectEditorState::Save: failed to write {}", path.string());
}

void ProjectEditorState::AddRecentScene(const std::string& absolutePath, const CHEngine::Project& proj)
{
    const std::string rel = proj.ToRelativePath(absolutePath);
    auto it = std::find(m_RecentScenes.begin(), m_RecentScenes.end(), rel);
    if (it != m_RecentScenes.end())
        m_RecentScenes.erase(it);
    m_RecentScenes.insert(m_RecentScenes.begin(), rel);
    if (static_cast<int>(m_RecentScenes.size()) > k_MaxRecentScenes)
        m_RecentScenes.resize(k_MaxRecentScenes);
}

} // namespace Sandbox
