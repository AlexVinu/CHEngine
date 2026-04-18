#pragma once

#include <CHEngine.h>
#include <CHEngine/Scene/RecentFiles.h>
#include <CHEngine/World/World.h>

#include <string>

namespace Sandbox {

/// Persists the scene attached to a World as JSON (.chscene). Tracks recent paths on disk.
/// Uses nlohmann::json via CHEngine::SceneSerializer. No UI or editor subsystems.
class SceneSerializer
{
public:
    SceneSerializer();

    /// Serializes \p world.GetScene() to \p path (pretty-printed JSON). Updates recent list on success.
    bool Save(CHEngine::World& world, const std::string& path);

    /// Loads a scene from \p path. Returns null on failure. Updates recent list on success.
    CHEngine::Scope<CHEngine::Scene> Load(const std::string& path);

    CHEngine::RecentFiles& GetRecentFiles() { return m_RecentFiles; }
    const CHEngine::RecentFiles& GetRecentFiles() const { return m_RecentFiles; }

private:
    static constexpr const char* k_RecentScenesListPath = "recent_scenes.txt";

    CHEngine::RecentFiles m_RecentFiles;
};

} // namespace Sandbox
