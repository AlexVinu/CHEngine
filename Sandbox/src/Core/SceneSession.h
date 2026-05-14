#pragma once

#include "CheStl/MemoryTypes.h"
#include "CHEngine/Scene/Scene.h"
#include "CHEngine/World/World.h"
#include "CHEngine/Camera/EditorCamera.h"
#include <boost/uuid/nil_generator.hpp>

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
        EditorScene = CHEngine::CreateRef<CHEngine::Scene>();
        ActiveScene = CHEngine::CreateRef<CHEngine::Scene>();
        RuntimeWorld = CHEngine::MakeScope<CHEngine::World>(EditorScene);
        ViewportCamera = CHEngine::MakeScope<CHEngine::EditorCamera>();
        ViewportCamera->SetViewportSize(ViewportSize.x, ViewportSize.y);
        ViewportCamera->SetYaw(glm::radians(-45.0f));
        ViewportCamera->SetPitch(glm::radians(25.0f));
    }

    State SessionState = State::Edit;

    CHEngine::Ref<CHEngine::Scene> EditorScene;

    CHEngine::Scope<CHEngine::EditorCamera> ViewportCamera;
    CHEngine::EntityHandle SelectedEntity{};

    glm::vec2 ViewportSize{ 1280.0f, 720.0f };
protected:
    CHEngine::Ref<CHEngine::Scene> ActiveScene;
    CHEngine::Scope<CHEngine::World> RuntimeWorld;
};