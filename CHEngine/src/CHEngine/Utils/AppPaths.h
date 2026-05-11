#pragma once

#include <Core.h>

#include <filesystem>

namespace CHEngine {

/// SDK paths anchored at the executable's directory (not cwd).
/// Use these for engine-owned resources: shaders, editor defaults, engine.json.
/// Project assets should resolve through ProjectManager::Current()->AssetsAbsPath().
class CHENGINE_API AppPaths
{
public:
    /// Directory containing the running executable. Cached after first call.
    static const std::filesystem::path& ExecutableDir();

    /// <ExecutableDir>/shaders — engine + editor shaders.
    static std::filesystem::path ShadersDir();

    /// <ExecutableDir>/editor_assets — minimal editor fallbacks (default texture, etc.).
    static std::filesystem::path EditorAssetsDir();

    /// <ExecutableDir>/engine.json — global editor preferences (last project, renderer).
    static std::filesystem::path EngineConfigPath();
};

} // namespace CHEngine
