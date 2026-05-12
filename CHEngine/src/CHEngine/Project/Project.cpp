#include "chepch.h"
#include "Project.h"

#include "CHEngine/Utils/AppPaths.h"

#include <FileSystem/FileSystem.h>
#include <Log/Log.h>
#include <nlohmann/json.hpp>
#include <filesystem>

namespace fs = std::filesystem;

namespace CHEngine {

fs::path Project::RootDir() const
{
    return fs::path(m_Path).parent_path();
}

fs::path Project::AssetsAbsPath() const
{
    return RootDir() / m_AssetsDir;
}

fs::path Project::ScenesAbsPath() const
{
    return RootDir() / m_ScenesDir;
}

std::string Project::ToRelativePath(const std::string& absolutePath) const
{
    try {
        return fs::relative(fs::path(absolutePath), RootDir()).string();
    } catch (...) {
        return absolutePath;
    }
}

void Project::SyncEditorAssets() const
{
    const fs::path src = AppPaths::EditorAssetsDir();
    const fs::path dst = AssetsAbsPath();

    std::error_code ec;
    if (!fs::exists(src, ec) || !fs::is_directory(src, ec))
    {
        CHE_CORE_WARN("Project::SyncEditorAssets: editor_assets dir not found at '{}'", src.string());
        return;
    }

    fs::create_directories(dst, ec);
    if (ec)
    {
        CHE_CORE_ERROR("Project::SyncEditorAssets: failed to create '{}': {}", dst.string(), ec.message());
        return;
    }

    const auto opts = fs::copy_options::recursive | fs::copy_options::skip_existing;
    fs::copy(src, dst, opts, ec);
    if (ec)
    {
        CHE_CORE_WARN("Project::SyncEditorAssets: copy reported '{}'", ec.message());
        return;
    }

    CHE_CORE_INFO("Project::SyncEditorAssets: synced '{}' -> '{}'", src.string(), dst.string());
}

std::string Project::ResolveScenePath(const std::string& relPath) const
{
    if (relPath.empty())
        return {};
    if (fs::path(relPath).is_absolute())
        return relPath;
    return (RootDir() / relPath).string();
}

std::unique_ptr<Project> Project::Load(const std::string& path)
{
    if (path.empty() || !FileSystem::Exists(fs::path(path)))
    {
        CHE_CORE_WARN("Project::Load: file not found: {}", path);
        return nullptr;
    }
    const std::string text = FileSystem::ReadFileText(fs::path(path));
    if (text.empty())
    {
        CHE_CORE_WARN("Project::Load: empty file: {}", path);
        return nullptr;
    }
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(text);
    } catch (const std::exception& e) {
        CHE_CORE_ERROR("Project::Load: JSON parse error: {}", e.what());
        return nullptr;
    }

    auto proj            = std::unique_ptr<Project>(new Project());
    proj->m_Path         = fs::absolute(fs::path(path)).string();
    proj->m_Name         = j.value("name", "Unnamed");
    proj->m_EngineVersion = j.value("engine_version", "0.1");
    proj->m_StartupScene = j.value("startup_scene", "");
    proj->m_AssetsDir    = j.value("assets_dir", "Assets");
    proj->m_ScenesDir    = j.value("scenes_dir", "Scenes");

    CHE_CORE_INFO("Project loaded: {} ({})", proj->m_Name, proj->m_Path);
    return proj;
}

bool Project::Save() const
{
    nlohmann::json j;
    j["name"]           = m_Name;
    j["engine_version"] = m_EngineVersion;
    j["startup_scene"]  = m_StartupScene;
    j["assets_dir"]     = m_AssetsDir;
    j["scenes_dir"]     = m_ScenesDir;

    const std::string dump = j.dump(2) + "\n";
    if (!FileSystem::WriteFileText(fs::path(m_Path), dump))
    {
        CHE_CORE_ERROR("Project::Save: failed to write {}", m_Path);
        return false;
    }
    return true;
}

std::unique_ptr<Project> Project::Create(const std::string& parentDir, const std::string& name)
{
    if (parentDir.empty() || name.empty())
        return nullptr;

    const fs::path rootDir = fs::path(parentDir) / name;
    std::error_code ec;
    fs::create_directories(rootDir / "Scenes",            ec);
    fs::create_directories(rootDir / "Assets" / "Models", ec);
    fs::create_directories(rootDir / "Assets" / "Textures", ec);
    if (ec)
    {
        CHE_CORE_ERROR("Project::Create: failed to create directories: {}", ec.message());
        return nullptr;
    }

    FileSystem::WriteFileText(rootDir / "Scenes" / "Main.chscene",
                              R"({"version":1,"entities":[],"meta":{}})");

    auto proj            = std::unique_ptr<Project>(new Project());
    proj->m_Name         = name;
    proj->m_Path         = (rootDir / (name + ".cheproj")).string();
    proj->m_StartupScene = "Scenes/Main.chscene";

    if (!proj->Save())
    {
        CHE_CORE_ERROR("Project::Create: failed to save project file");
        return nullptr;
    }

    CHE_CORE_INFO("Project created: {} ({})", proj->m_Name, proj->m_Path);
    return proj;
}

} // namespace CHEngine
