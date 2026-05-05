#include "SceneViewLayer_Play.h"

#include "SceneViewLayer.h"
#include "SceneViewLayerAccess.h"

#include <CHEngine/World/ISystem.h>
#include <CHEngine/World/World.h>
#include <CHEngine/Scene/Components.h>

void SceneViewLayerPlay::EnterPlayMode(SceneViewLayer& layer)
{
    EditorWorldContext& activeSession = SceneViewLayerAccess::Active(layer);
    if (activeSession.SessionState != SceneSession::State::Edit)
        return;

    activeSession.CommandStack.Clear();
    activeSession.ActivateActiveScene();

    // Устанавливаем viewport size всем CameraComponent в сцене.
    // SceneCamera по умолчанию имеет m_AspectRatio = 0 — без этого
    // glm::perspective/ortho генерирует нулевую матрицу → серый экран.
    const glm::vec2& vpSize = activeSession.ViewportSize;
    // RuntimeWorld уже переключён на ActiveScene внутри ActivateActiveScene().
    // Получаем сцену через World чтобы не обращаться к protected ActiveScene.
    if (vpSize.x > 1.0f && vpSize.y > 1.0f && activeSession.GetRuntimeWorld())
    {
        auto w = static_cast<uint32_t>(vpSize.x);
        auto h = static_cast<uint32_t>(vpSize.y);

        auto scene = activeSession.GetRuntimeWorld()->GetSceneRef();
        if (scene)
        {
            scene->ForEach<CHEngine::CameraComponent>(
                [&](CHEngine::EntityHandle, const CHEngine::UUID&, CHEngine::CameraComponent& cam)
                {
                    cam.Camera.SetViewportSize(w, h);
                });
        }
    }

    activeSession.SessionState = SceneSession::State::Play;
}

void SceneViewLayerPlay::EnterPauseMode(SceneViewLayer& layer)
{
    EditorWorldContext& activeSession = SceneViewLayerAccess::Active(layer);
    if (activeSession.SessionState != SceneSession::State::Play)
        return;

    activeSession.SessionState = SceneSession::State::Pause;
}

void SceneViewLayerPlay::ResumeFromPause(SceneViewLayer& layer)
{
    EditorWorldContext& activeSession = SceneViewLayerAccess::Active(layer);
    if (activeSession.SessionState != SceneSession::State::Pause)
        return;

    activeSession.SessionState = SceneSession::State::Play;
}

void SceneViewLayerPlay::StopPlayMode(SceneViewLayer& layer)
{
    EditorWorldContext& activeSession = SceneViewLayerAccess::Active(layer);
    if (activeSession.SessionState == SceneSession::State::Edit)
        return;

    activeSession.ActivateEditorScene();
    activeSession.SelectedEntity = {};
    activeSession.SessionState = SceneSession::State::Edit;
}

//void SceneViewLayerPlay::StepOneFrame(SceneViewLayer& layer)
//{
//    EditorWorldContext& activeSession = SceneViewLayerAccess::Active(layer);
//    if (activeSession.SessionState != SceneSession::State::Pause)
//        return;
//
//    CHE_CORE_ASSERT(activeSession.RuntimeWorld, "SceneViewLayer: RuntimeWorld must exist");
//    CHEngine::World* runtime_world = activeSession.RuntimeWorld.get();
//    if (!runtime_world)
//        return;
//
//    runtime_world->SetState(CHEngine::WorldState::Simulating);
//    runtime_world->Update(CHEngine::Timestep(activeSession.StepDt));
//    runtime_world->SetState(CHEngine::WorldState::Presenting);
//}
