#include "chepch.h"
#include "ProjectManager.h"

#include "CHEngine/Utils/AppPaths.h"

#include <FileSystem/FileSystem.h>
#include <Log/Log.h>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

// TODO: Make constants

// PROJECT MANAGER
ProjectManager::ProjectManager()
	:m_CurrentProject(nullptr)
{

}

ProjectManager::~ProjectManager()
{

}

bool ProjectManager::Open(const std::string& projPath)
{
    if (projPath.empty())
        return false;

    if (!CHEngine::FileSystem::Exists(fs::path(projPath)))
    {
        CHE_CORE_WARN("ProjectManager::Open: file not found: {}", projPath);
        return false;
    }
    const std::string text = CHEngine::FileSystem::ReadFileText(fs::path(projPath));
    if (text.empty())
    {
        CHE_CORE_WARN("ProjectManager::Open: empty file: {}", projPath);
        return false;
    }
    nlohmann::json j;

    try {
        j = nlohmann::json::parse(text);
		if (j.contains("recent_scenes") && j["recent_scenes"].is_array())
		{
            m_RecentScenes.clear();
			for (const auto& s : j["recent_scenes"])
				if (s.is_string())
					m_RecentScenes.push_back(s.get<std::string>());
		}
    } catch (const std::exception& e) {
        CHE_CORE_ERROR("ProjectManager::Open: JSON parse error: {}", e.what());
        return false;
    }

    auto proj             = std::unique_ptr<Project>(new Project());
    proj->m_Path          = fs::absolute(fs::path(projPath)).string();
    proj->m_Name          = j.value("name", "Unnamed");
    proj->m_EngineVersion = j.value("engine_version", "0.1");
    proj->m_StartupScene  = j.value("startup_scene", "");
    proj->m_AssetsDir     = j.value("assets_dir",  "Assets");
    proj->m_ScenesDir     = j.value("scenes_dir",  "Scenes");
    proj->m_ScriptsDir    = j.value("scripts_dir", "Assets/Scripts");
    proj->m_ShadersDir    = j.value("shaders_dir", "Assets/Shaders");

    CHE_CORE_INFO("Project loaded: {} ({})", proj->m_Name, proj->m_Path);
    m_CurrentProject = std::move(proj);
    m_CurrentProject->SyncEditorAssets();
    return true;
}

std::string ProjectManager::Create(const std::string& parentDir, const std::string& name)
{
    if (parentDir.empty() || name.empty())
        return {};

    const fs::path rootDir = fs::path(parentDir) / name;
    std::error_code ec;
    fs::create_directories(rootDir / "Scenes",               ec);
    fs::create_directories(rootDir / "Assets" / "Models",   ec);
    fs::create_directories(rootDir / "Assets" / "Textures", ec);
    fs::create_directories(rootDir / "Assets" / "Scripts",  ec);
    fs::create_directories(rootDir / "Assets" / "Shaders",  ec);
    if (ec)
    {
        CHE_CORE_ERROR("ProjectManager::Create: failed to create directories: {}", ec.message());
        return {};
    }

    CHEngine::FileSystem::WriteFileText(rootDir / "Scenes" / "Main.chscene",
                              R"({"version":1,"entities":[],"meta":{}})");

    auto proj            = std::unique_ptr<Project>(new Project());
    proj->m_Name         = name;
    proj->m_Path         = (rootDir / (name + ".cheproj")).string();
    proj->m_StartupScene = "Scenes/Main.chscene";

    if (!proj->Save())
    {
        CHE_CORE_ERROR("ProjectManager::Create: failed to save project file");
        return {};
    }

    CHE_CORE_INFO("Project created: {} ({})", proj->m_Name, proj->m_Path);
    const std::string path = proj->GetPath();
    m_CurrentProject = std::move(proj);
    m_CurrentProject->SyncEditorAssets();
    return path;
}

void ProjectManager::Close()
{
    m_CurrentProject.reset();
}

Project* ProjectManager::Current()
{
    return m_CurrentProject.get();
}

bool ProjectManager::HasProject()
{
    return m_CurrentProject != nullptr;
}

void ProjectManager::Save() const
{
	nlohmann::json j;
	nlohmann::json arr = nlohmann::json::array();
	for (const auto& s : m_RecentScenes)
		arr.push_back(s);
	j["recent_scenes"] = arr;

	const fs::path path = m_CurrentProject->RootDir() / ".editor_state.json";
	if (!CHEngine::FileSystem::WriteFileText(path, j.dump(2) + "\n"))
		CHE_CORE_WARN("ProjectEditorState::Save: failed to write {}", path.string());
}

void ProjectManager::AddRecentScene(const std::string& absolutePath)
{
	const std::string rel = m_CurrentProject->ToRelativePath(absolutePath);
	auto it = std::find(m_RecentScenes.begin(), m_RecentScenes.end(), rel);
	if (it != m_RecentScenes.end())
		m_RecentScenes.erase(it);
	m_RecentScenes.insert(m_RecentScenes.begin(), rel);
	if (static_cast<int>(m_RecentScenes.size()) > k_MaxRecentScenes)
		m_RecentScenes.resize(k_MaxRecentScenes);
}
