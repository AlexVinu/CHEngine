#include "chepch.h"
#include "ProjectManager.h"

#include <Log/Log.h>

namespace CHEngine {

static std::unique_ptr<Project> s_Current;

bool ProjectManager::Open(const std::string& projPath)
{
    if (projPath.empty())
        return false;
    auto proj = Project::Load(projPath);
    if (!proj)
        return false;
    s_Current = std::move(proj);
    s_Current->SyncEditorAssets();
    return true;
}

std::string ProjectManager::Create(const std::string& parentDir, const std::string& name)
{
    auto proj = Project::Create(parentDir, name);
    if (!proj)
        return {};
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
