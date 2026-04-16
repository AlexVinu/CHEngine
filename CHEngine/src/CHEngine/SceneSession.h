#pragma once

#include "CheStl/MemoryTypes.h"
#include "Scene/Scene.h"
#include "World/World.h"
#include "Camera/EditorCamera.h"
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
};