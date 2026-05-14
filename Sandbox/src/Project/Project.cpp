#include "chepch.h"
#include "Project.h"

#include "CHEngine/Utils/AppPaths.h"

#include <FileSystem/FileSystem.h>
#include <Log/Log.h>
#include <nlohmann/json.hpp>
#include <filesystem>

namespace fs = std::filesystem;


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

fs::path Project::ScriptsAbsPath() const
{
    return RootDir() / m_ScriptsDir;
}

fs::path Project::ShadersAbsPath() const
{
    return RootDir() / m_ShadersDir;
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
    const fs::path src = CHEngine::AppPaths::EditorAssetsDir();
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

std::string Project::CreateScene(const std::string& name)
{
    std::error_code ec;
    const fs::path scenesDir = ScenesAbsPath();
    fs::create_directories(scenesDir, ec);

    fs::path target;
    if (!name.empty())
    {
        target = scenesDir / (fs::path(name).stem().string() + ".chscene");
    }
    else
    {
        for (int i = 1; i < 10000; ++i)
        {
            target = scenesDir / ("Untitled_" + std::to_string(i) + ".chscene");
            if (!fs::exists(target))
                break;
        }
    }

    if (!CHEngine::FileSystem::WriteFileText(target, R"({"version":3,"objects":[]})"))
    {
        CHE_CORE_ERROR("Project::CreateScene: failed to write '{}'", target.string());
        return {};
    }

    const std::string rel = fs::relative(target, RootDir(), ec).generic_string();
    CHE_CORE_INFO("Project::CreateScene: created '{}'", rel);
    return rel;
}

bool Project::DeleteScene(const std::string& relPath)
{
    if (relPath.empty())
        return false;

    std::error_code ec;
    fs::remove(RootDir() / relPath, ec);
    if (ec)
    {
        CHE_CORE_ERROR("Project::DeleteScene: failed to remove '{}': {}", relPath, ec.message());
        return false;
    }

    if (m_StartupScene == relPath)
    {
        m_StartupScene.clear();
        Save();
    }

    CHE_CORE_INFO("Project::DeleteScene: removed '{}'", relPath);
    return true;
}

std::string Project::RenameScene(const std::string& relPath, const std::string& newName)
{
    if (relPath.empty() || newName.empty())
        return {};

    const fs::path oldAbs = RootDir() / relPath;
    const fs::path newAbs = oldAbs.parent_path() / (fs::path(newName).stem().string() + ".chscene");

    if (newAbs == oldAbs)
        return relPath;

    std::error_code ec;
    if (fs::exists(newAbs))
    {
        CHE_CORE_WARN("Project::RenameScene: target already exists: '{}'", newAbs.string());
        return {};
    }

    fs::rename(oldAbs, newAbs, ec);
    if (ec)
    {
        CHE_CORE_ERROR("Project::RenameScene: failed: {}", ec.message());
        return {};
    }

    const std::string newRel = fs::relative(newAbs, RootDir(), ec).generic_string();

    if (m_StartupScene == relPath)
    {
        m_StartupScene = newRel;
        Save();
    }

    CHE_CORE_INFO("Project::RenameScene: '{}' -> '{}'", relPath, newRel);
    return newRel;
}

bool Project::Save() const
{
    nlohmann::json j;
    j["name"]           = m_Name;
    j["engine_version"] = m_EngineVersion;
    j["startup_scene"]  = m_StartupScene;
    j["assets_dir"]     = m_AssetsDir;
    j["scenes_dir"]     = m_ScenesDir;
    j["scripts_dir"]    = m_ScriptsDir;
    j["shaders_dir"]    = m_ShadersDir;

    const std::string dump = j.dump(2) + "\n";
    if (!CHEngine::FileSystem::WriteFileText(fs::path(m_Path), dump))
    {
        CHE_CORE_ERROR("Project::Save: failed to write {}", m_Path);
        return false;
    }
    return true;
}
