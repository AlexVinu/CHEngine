#include "chepch.h"
#include "ProjectManager.h"

#include "CHEngine/Utils/AppPaths.h"

#include <FileSystem/FileSystem.h>
#include <Log/Log.h>
#include <nlohmann/json.hpp>
#include <filesystem>

namespace fs = std::filesystem;

namespace CHEngine {

static std::unique_ptr<Project> s_Current;

bool ProjectManager::Open(const std::string& projPath)
{
    if (projPath.empty())
        return false;

    if (!FileSystem::Exists(fs::path(projPath)))
    {
        CHE_CORE_WARN("ProjectManager::Open: file not found: {}", projPath);
        return false;
    }
    const std::string text = FileSystem::ReadFileText(fs::path(projPath));
    if (text.empty())
    {
        CHE_CORE_WARN("ProjectManager::Open: empty file: {}", projPath);
        return false;
    }
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(text);
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
    s_Current = std::move(proj);
    s_Current->SyncEditorAssets();
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

    // Must match SceneSerializer kSceneFormatVersion=3 with "objects" array.
    FileSystem::WriteFileText(rootDir / "Scenes" / "Main.chscene",
                              R"({"version":3,"objects":[]})");

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
    s_Current = std::move(proj);
    s_Current->SyncEditorAssets();
    return path;
}

void ProjectManager::Close()
{
    s_Current.reset();
}

Project* ProjectManager::Current()
{
    return s_Current.get();
}

bool ProjectManager::HasProject()
{
    return s_Current != nullptr;
}

} // namespace CHEngine
