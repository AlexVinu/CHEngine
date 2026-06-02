#include "EditorInputLayer.h"

#include "EditorContext.h"
#include "EditorCameraController.h"
#include "GizmoSystem.h"

EditorInputLayer::EditorInputLayer(Sandbox::EditorContext& ctx)
    : Layer("EditorInput"), m_Ctx(ctx)
{
}

void EditorInputLayer::OnEvent(CHEngine::Event& e)
{
    m_Ctx.CameraController.OnEvent(e);
    m_Ctx.Gizmo.OnEvent(e);
}
