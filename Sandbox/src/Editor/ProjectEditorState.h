#pragma once

#include <string>
#include <vector>

namespace CHEngine { class Project; }

namespace Sandbox {

/// Editor-only state for a project, stored in <project_root>/.editor_state.json.
/// Not part of CHEngine runtime — keeps project file clean.
class ProjectEditorState
{
public:
    static ProjectEditorState LoadFor(const CHEngine::Project& proj);
    void Save(const CHEngine::Project& proj) const;

    void AddRecentScene(const std::string& absolutePath, const CHEngine::Project& proj);
    const std::vector<std::string>& GetRecentScenes() const { return m_RecentScenes; }

private:
    static constexpr int k_MaxRecentScenes = 10;
    std::vector<std::string> m_RecentScenes; // relative to project root
};

} // namespace Sandbox
