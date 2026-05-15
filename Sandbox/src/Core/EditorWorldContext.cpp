#include "EditorWorldContext.h"

#include <CHEngine/World/World.h>
#include <Log/Log.h>

#include <algorithm>
#include <filesystem>

EditorWorldContext::EditorWorldContext()
    : SceneSession()
{
	UpdateState(std::nullopt, std::nullopt);
}

void EditorWorldContext::UpdateState(std::optional<SceneSession::State> new_state, std::optional<bool> is_active)
{
	CHE_CORE_ASSERT(EditorScene, "EditorWorldContext must have EditorScene");
    m_IsActive = is_active.value_or(m_IsActive);
	m_SessionState = new_state.value_or(m_SessionState);

	// Ensure scene is set when becoming active
	if (m_IsActive && !RuntimeWorld->GetSceneRef())
	{
		ActivateEditorScene();
	}

	switch (m_SessionState)
	{
	case SceneSession::State::Edit:
		if (!m_IsActive) return;
		RuntimeWorld->SetState(CHEngine::WorldState::Presenting);
		RuntimeWorld->SetActiveCamera(ViewportCamera.get());
		break;

	case SceneSession::State::Play:
		if (m_IsActive)
		{
			RuntimeWorld->SetState(CHEngine::WorldState::Simulating);
			RuntimeWorld->SetActiveCamera(nullptr);
		}
		else
			RuntimeWorld->SetState(CHEngine::WorldState::SimulatingWithoutPresenting);

		break;

	case SceneSession::State::Pause:
		if (!m_IsActive) return;
		RuntimeWorld->SetState(CHEngine::WorldState::Presenting);
		RuntimeWorld->SetActiveCamera(nullptr);
		break;
	}
}

void EditorWorldContext::ActivateActiveScene()
{
    CHE_ASSERT(ActiveScene, "THERE ARE NO ACTIVE SCENE");
    CHE_ASSERT(EditorScene, "THERE ARE NO EDITOR SCENE");
    CHE_ASSERT(RuntimeWorld, "THERE ARE NO RuntimeWorld");

    *ActiveScene = *EditorScene;
    RuntimeWorld->SetScene(ActiveScene);
}

void EditorWorldContext::ActivateEditorScene()
{
    CHE_ASSERT(ActiveScene, "THERE ARE NO ACTIVE SCENE");
    CHE_ASSERT(EditorScene, "THERE ARE NO EDITOR SCENE");
    CHE_ASSERT(RuntimeWorld, "THERE ARE NO RuntimeWorld");

    *ActiveScene = *EditorScene;
    RuntimeWorld->SetScene(EditorScene);
}

std::string EditorWorldContext::DisplayName() const
{
    if (SceneRelPath.empty())
        return "Untitled";
    std::string name = std::filesystem::path(SceneRelPath).stem().string();
    std::replace(name.begin(), name.end(), '_', ' ');
    return name;
}
