#pragma once

#include "CheStl/MemoryTypes.h"
#include "CHEngine/Scene/Scene.h"
#include "CHEngine/World/World.h"
#include "CHEngine/Camera/EditorCamera.h"
#include <boost/uuid/nil_generator.hpp>

struct SceneSession
{
    enum class State
    {
        Edit,
        Simulate,
        Play,
        Pause
    };

    State SessionState = State::Edit;

    CHEngine::Ref<CHEngine::Scene> EditorScene;
    CHEngine::Ref<CHEngine::Scene> ActiveScene;
    CHEngine::Scope<CHEngine::World> RuntimeWorld;

    CHEngine::Scope<CHEngine::EditorCamera> ViewportCamera;
    CHEngine::EntityHandle SelectedEntity{};

    glm::vec2 ViewportSize{ 1280.0f, 720.0f };

    SceneSession()
    {
        EditorScene = CHEngine::CreateRef<CHEngine::Scene>();
        ActiveScene = EditorScene;
        RuntimeWorld = CHEngine::MakeScope<CHEngine::World>(EditorScene.get());
        ViewportCamera = CHEngine::MakeScope<CHEngine::EditorCamera>();
        ViewportCamera->SetViewportSize(ViewportSize.x, ViewportSize.y);
        ViewportCamera->SetPitch(glm::radians(-30.0f));
    }
};