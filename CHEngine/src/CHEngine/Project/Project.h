#pragma once
#include <Core.h>
#include <string>
#include <filesystem>
#include <memory>

namespace CHEngine {

class CHENGINE_API Project
{
    friend class ProjectManager;
public:
    bool Save() const;

    const std::string& GetName()     const { return m_Name; }
    const std::string& GetPath()     const { return m_Path; }
    const std::string& GetStartupScene() const { return m_StartupScene; }
    void SetStartupScene(const std::string& relPath) { m_StartupScene = relPath; }

    std::filesystem::path RootDir()        const;
    std::filesystem::path AssetsAbsPath()  const;
    std::filesystem::path ScenesAbsPath()  const;
    std::filesystem::path ScriptsAbsPath() const;
    std::filesystem::path ShadersAbsPath() const;

    std::string ToRelativePath(const std::string& absolutePath) const;
    std::string ResolveScenePath(const std::string& relPath)    const;

    // Scene file management (paths relative to RootDir)
    std::string CreateScene(const std::string& name = "");
    bool        DeleteScene(const std::string& relPath);
    std::string RenameScene(const std::string& relPath, const std::string& newName);

    /// Copy contents of <exe>/editor_assets/ into this project's Assets/ directory.
    /// Non-destructive: existing files are skipped (skip_existing). Safe to call repeatedly.
    void SyncEditorAssets() const;

private:
    Project() = default;

    std::string m_Path;
    std::string m_Name;
    std::string m_EngineVersion = "0.1";
    std::string m_StartupScene;
    std::string m_AssetsDir  = "Assets";
    std::string m_ScenesDir  = "Scenes";
    std::string m_ScriptsDir = "Assets/Scripts";
    std::string m_ShadersDir = "Assets/Shaders";
};

} // namespace CHEngine
