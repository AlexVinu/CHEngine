#include "EditorWorldContext.h"

#include <CHEngine/World/World.h>
#include <Log/Log.h>

#include <algorithm>
#include <filesystem>

EditorWorldContext::EditorWorldContext(CHEngine::WorldsList* list)
    : SceneSession(list)
{
	UpdateState(std::nullopt, std::nullopt);
}

void EditorWorldContext::UpdateState(std::optional<SceneSession::State> new_state, std::optional<bool> is_active)
{
	CHE_CORE_ASSERT(EditorScene, "EditorWorldContext must have EditorScene");
    m_IsActive = is_active.value_or(m_IsActive);
	m_SessionState = new_state.value_or(m_SessionState);

	// Ensure scene is set when becoming active
	if (m_IsActive && !GetSceneRef())
	{
		ActivateEditorScene();
	}

	switch (m_SessionState)
	{
	case SceneSession::State::Edit:
		if (!m_IsActive)
		{
			SetState(CHEngine::WorldState::NONE);
			return;
		}
		SetState(CHEngine::WorldState::Presenting);
		SetActiveCamera(ViewportCamera.get());
		break;

	case SceneSession::State::Play:
		if (m_IsActive)
		{
			SetState(CHEngine::WorldState::Simulating);
			SetActiveCamera(nullptr);
		}
		else
			SetState(CHEngine::WorldState::SimulatingWithoutPresenting);

		break;

	case SceneSession::State::Pause:
		if (!m_IsActive) return;
		SetState(CHEngine::WorldState::Presenting);
		SetActiveCamera(nullptr);
		break;
	}
}

void EditorWorldContext::ActivateActiveScene()
{
    CHE_ASSERT(ActiveScene, "THERE ARE NO ACTIVE SCENE");
    CHE_ASSERT(EditorScene, "THERE ARE NO EDITOR SCENE");

    *ActiveScene = *EditorScene;
    SetScene(ActiveScene);
}

void EditorWorldContext::ActivateEditorScene()
{
    CHE_ASSERT(ActiveScene, "THERE ARE NO ACTIVE SCENE");
    CHE_ASSERT(EditorScene, "THERE ARE NO EDITOR SCENE");

    *ActiveScene = *EditorScene;
    SetScene(EditorScene);
}
