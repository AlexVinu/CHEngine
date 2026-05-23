#pragma once

#include "CheStl/MemoryTypes.h"
#include "CHEngine/Scene/Scene.h"
#include "CHEngine/World/World.h"
#include "CHEngine/Camera/EditorCamera.h"
#include <functional>

#include <optional>

// Basic structure to store the scene session data
struct SceneSession : public CHEngine::World
{
    enum class State
    {
        Edit,
        Play,
        Pause
    };

    SceneSession(CHEngine::WorldsList* list)
        : CHEngine::World(list)
    {
        EditorScene = CreateRef<CHEngine::Scene>();
        ActiveScene = CreateRef<CHEngine::Scene>();
        ViewportCamera = MakeScope<CHEngine::EditorCamera>();
        ViewportCamera->SetViewportSize(ViewportSize.x, ViewportSize.y);
        ViewportCamera->SetYaw(glm::radians(-45.0f));
        ViewportCamera->SetPitch(glm::radians(25.0f));
        SetScene(EditorScene);
    }

    virtual ~SceneSession() = default;

    Ref<CHEngine::Scene> EditorScene;

    Scope<CHEngine::EditorCamera> ViewportCamera;
    CHEngine::EntityHandle SelectedEntity{};
    glm::vec2 ViewportSize{ 1280.0f, 720.0f };

    Ref<CHEngine::Scene> ActiveScene;

    State GetSessionState() const { return m_SessionState; }
protected:
    State m_SessionState = State::Edit;
};