#include "SceneViewLayer_Play.h"

#include "SceneViewLayer.h"
#include "SceneViewLayerAccess.h"

#include <CHEngine/Application.h>

#include <CHEngine/World/ISystem.h>
#include <CHEngine/World/World.h>

void SceneViewLayerPlay::EnterPlayMode(SceneViewLayer& layer)
{
    Ref<EditorWorldContext> ctx = SceneViewLayerAccess::ActiveRef(layer);
    if (ctx->GetSessionState() != SceneSession::State::Edit)
        return;

    ctx->CommandStack.Clear();

    // One world = one scene. Play just runs a copy of the current tab's scene.
    ctx->ActivateActiveScene();
    ctx->UpdateState(SceneSession::State::Play);
    ctx->RuntimeWorld->SetActiveCamera(nullptr);

    if (!CHEngine::Application::Get().InputSystem()->IsActiveContext("Game"))
        CHEngine::Application::Get().InputSystem()->PushContext("Game");
}

void SceneViewLayerPlay::EnterPauseMode(SceneViewLayer& layer)
{
    Ref<EditorWorldContext> activeSession = SceneViewLayerAccess::ActiveRef(layer);
    if (activeSession->GetSessionState() != SceneSession::State::Play)
        return;

    activeSession->UpdateState(SceneSession::State::Pause);
}

void SceneViewLayerPlay::ResumeFromPause(SceneViewLayer& layer)
{
    Ref<EditorWorldContext> activeSession = SceneViewLayerAccess::ActiveRef(layer);
    if (activeSession->GetSessionState() != SceneSession::State::Pause)
        return;

    activeSession->UpdateState(SceneSession::State::Play);
}

void SceneViewLayerPlay::StopPlayMode(SceneViewLayer& layer)
{
    Ref<EditorWorldContext> ctx = SceneViewLayerAccess::ActiveRef(layer);
    if (ctx->GetSessionState() == SceneSession::State::Edit)
        return;

    ctx->ActivateEditorScene();
    ctx->SelectedEntity = {};
    ctx->UpdateState(SceneSession::State::Edit);
    ctx->RuntimeWorld->SetActiveCamera(ctx->ViewportCamera.get());

    CHEngine::Application::Get().InputSystem()->PopContext("Game");
}

