#pragma once

#include "CheStl/MemoryTypes.h"
#include "CHEngine/Scene/Scene.h"
#include "CHEngine/World/World.h"
#include "CHEngine/Camera/EditorCamera.h"
#include <boost/uuid/nil_generator.hpp>

#include <functional>

#include <optional>

// Basic structure to store the scene session data
struct SceneSession
{
    enum class State
    {
        Edit,
        Play,
        Pause
    };

    SceneSession()
    {
        EditorScene = CreateRef<CHEngine::Scene>();
        ActiveScene = CreateRef<CHEngine::Scene>();
        RuntimeWorld = CreateRef<CHEngine::World>(EditorScene);
        ViewportCamera = MakeScope<CHEngine::EditorCamera>();
        ViewportCamera->SetViewportSize(ViewportSize.x, ViewportSize.y);
        ViewportCamera->SetPitch(glm::radians(-30.0f));
    }

    Ref<CHEngine::Scene> EditorScene;

    Scope<CHEngine::EditorCamera> ViewportCamera;
    CHEngine::EntityHandle SelectedEntity{};
    glm::vec2 ViewportSize{ 1280.0f, 720.0f };

    Ref<CHEngine::Scene> ActiveScene;
    Ref<CHEngine::World> RuntimeWorld;

    State GetSessionState() const { return m_SessionState; }
protected:
    State m_SessionState = State::Edit;
};