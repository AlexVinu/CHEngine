#include "SceneViewLayer_Play.h"

#include "SceneViewLayer.h"
#include "SceneViewLayerAccess.h"

#include <CHEngine/Application.h>

#include <CHEngine/World/ISystem.h>
#include <CHEngine/World/World.h>

void SceneViewLayerPlay::EnterPlayMode(SceneViewLayer& layer)
{
    EditorWorldContext* ctx = SceneViewLayerAccess::ActiveWorldCtx(layer);
    if (ctx->GetSessionState() != SceneSession::State::Edit)
        return;

    ctx->CommandStack.Clear();

    // One world = one scene. Play just runs a copy of the current tab's scene.
    ctx->ActivateActiveScene();
    ctx->UpdateState(SceneSession::State::Play);

    if (!CHEngine::Application::Get().InputSystem()->IsActiveContext("Game"))
        CHEngine::Application::Get().InputSystem()->PushContext("Game");
}

void SceneViewLayerPlay::EnterPauseMode(SceneViewLayer& layer)
{
    EditorWorldContext* activeSession = SceneViewLayerAccess::ActiveWorldCtx(layer);
    if (activeSession->GetSessionState() != SceneSession::State::Play)
        return;

    activeSession->UpdateState(SceneSession::State::Pause);
}

void SceneViewLayerPlay::ResumeFromPause(SceneViewLayer& layer)
{
    EditorWorldContext* activeSession = SceneViewLayerAccess::ActiveWorldCtx(layer);
    if (activeSession->GetSessionState() != SceneSession::State::Pause)
        return;

    activeSession->UpdateState(SceneSession::State::Play);
}

void SceneViewLayerPlay::StopPlayMode(SceneViewLayer& layer)
{
    EditorWorldContext* ctx = SceneViewLayerAccess::ActiveWorldCtx(layer);
    if (ctx->GetSessionState() == SceneSession::State::Edit)
        return;

    ctx->ActivateEditorScene();
    ctx->SelectedEntity = {};
    ctx->UpdateState(SceneSession::State::Edit);

    CHEngine::Application::Get().InputSystem()->PopContext("Game");
}

