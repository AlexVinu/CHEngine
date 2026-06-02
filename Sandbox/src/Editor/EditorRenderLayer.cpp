#include "EditorRenderLayer.h"

#include "EditorContext.h"
#include "EditorViewport.h"
#include "GizmoSystem.h"
#include "ProjectManager.h"
#include "EditorWorldContext.h"

#include <Profiler.h>

EditorRenderLayer::EditorRenderLayer(Sandbox::EditorContext& ctx)
    : Layer("EditorRender"), m_Ctx(ctx)
{
}

void EditorRenderLayer::OnUpdate(CHEngine::Timestep /*dt*/)
{
    CHE_PROFILE_FUNCTION();

    if (!m_Ctx.Worlds || m_Ctx.Worlds->IsEmpty())
        return;

    EditorWorldContext* active = m_Ctx.ActiveCtx();
    if (!active)
        return;

    m_Ctx.Gizmo.BindCommandStack(&active->CommandStack);

    // Thats fine if we draw only one scene per update
    // TODO: Do it in proper way
    m_Ctx.Viewport.BeginSceneRender(active);
}
