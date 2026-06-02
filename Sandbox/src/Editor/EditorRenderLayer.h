#pragma once

#include <CHEngine/Layer/Layer.h>

namespace Sandbox { struct EditorContext; }

/// Слой отрисовки сцены в FBO.
///
/// Единственная ответственность — взять активную сессию и наполнить editor-viewport
/// готовой текстурой кадра. Не знает про ImGui и панели. Состоянием не владеет —
/// читает общий EditorContext (активная сессия, Viewport, Gizmo).
class EditorRenderLayer : public CHEngine::Layer
{
public:
    explicit EditorRenderLayer(Sandbox::EditorContext& ctx);
    void OnUpdate(CHEngine::Timestep dt) override;

private:
    Sandbox::EditorContext& m_Ctx;
};
