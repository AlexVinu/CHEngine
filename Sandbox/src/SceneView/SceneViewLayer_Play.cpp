#include "SceneViewLayer_Play.h"

#include "EditorContext.h"

#include <CHEngine/Application.h>

#include <CHEngine/World/ISystem.h>
#include <CHEngine/World/World.h>

void SceneViewLayerPlay::EnterPlayMode(Sandbox::EditorContext& ctx)
{
    EditorWorldContext* session = ctx.ActiveCtx();
    if (session->GetSessionState() != SceneSession::State::Edit)
        return;

    session->CommandStack.Clear();

    // One world = one scene. Play just runs a copy of the current tab's scene.
    session->ActivateActiveScene();
    session->UpdateState(SceneSession::State::Play);

    if (!CHEngine::Application::Get().InputSystem()->IsActiveContext("Game"))
        CHEngine::Application::Get().InputSystem()->PushContext("Game");
}

void SceneViewLayerPlay::EnterPauseMode(Sandbox::EditorContext& ctx)
{
    EditorWorldContext* activeSession = ctx.ActiveCtx();
    if (activeSession->GetSessionState() != SceneSession::State::Play)
        return;

    activeSession->UpdateState(SceneSession::State::Pause);
}

void SceneViewLayerPlay::ResumeFromPause(Sandbox::EditorContext& ctx)
{
    EditorWorldContext* activeSession = ctx.ActiveCtx();
    if (activeSession->GetSessionState() != SceneSession::State::Pause)
        return;

    activeSession->UpdateState(SceneSession::State::Play);
}

void SceneViewLayerPlay::StopPlayMode(Sandbox::EditorContext& ctx)
{
    EditorWorldContext* session = ctx.ActiveCtx();
    if (session->GetSessionState() == SceneSession::State::Edit)
        return;

    session->ActivateEditorScene();
    session->SelectedEntity = {};
    session->UpdateState(SceneSession::State::Edit);

    CHEngine::Application::Get().InputSystem()->PopContext("Game");
}
