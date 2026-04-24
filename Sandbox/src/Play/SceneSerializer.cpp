#include "SceneSerializer.h"

#include <CHEngine/Application.h>
#include <CHEngine/Scene/SceneSerializer.h>
#include <FileSystem/FileSystem.h>
#include <Log/Log.h>

#include <exception>

namespace Sandbox {

SceneSerializer::SceneSerializer()
{
    m_RecentFiles.LoadFromFile(k_RecentScenesListPath);
}

bool SceneSerializer::Save(CHEngine::Ref<CHEngine::Scene> editor_scene, const std::string& path)
{
    if (path.empty())
        return false;

    CHEngine::SceneSerializer engineSerializer{};
    nlohmann::json sceneJson = engineSerializer.SerializeToJson(editor_scene);

    if (!CHEngine::FileSystem::WriteFileText(path, sceneJson.dump(4)))
    {
        CHE_CORE_ERROR("Sandbox::SceneSerializer::Save: failed to write {}", path);
        return false;
    }

    m_RecentFiles.AddPath(path);
    m_RecentFiles.SaveToFile(k_RecentScenesListPath);
    CHE_CORE_INFO("Scene saved: {}", path);
    return true;
}

CHEngine::Ref<CHEngine::Scene> SceneSerializer::Load(const std::string& path)
{
    if (path.empty())
        return {};

    const std::string sceneText = CHEngine::FileSystem::ReadFileText(path);
    if (sceneText.empty())
    {
        CHE_CORE_ERROR("Sandbox::SceneSerializer::Load: cannot read {}", path);
        return {};
    }

    nlohmann::json sceneJson;
    try
    {
        sceneJson = nlohmann::json::parse(sceneText);
    }
    catch (const std::exception& e)
    {
        CHE_CORE_ERROR("Sandbox::SceneSerializer::Load: JSON parse error for {}: {}", path, e.what());
        return {};
    }

    auto* resources_ptr = CHEngine::Application::Get().GetRenderResources();
    if (!resources_ptr)
    {
        CHE_CORE_ERROR("Sandbox::SceneSerializer::Load: no render resources");
        return {};
    }

    CHEngine::SceneSerializer engineSerializer{};
    CHEngine::Ref<CHEngine::Scene> loadedScene = engineSerializer.DeserializeFromJson(sceneJson, *resources_ptr);
    if (!loadedScene)
        return {};

    m_RecentFiles.AddPath(path);
    m_RecentFiles.SaveToFile(k_RecentScenesListPath);
    CHE_CORE_INFO("Scene loaded: {}", path);
    return loadedScene;
}

} // namespace Sandbox
