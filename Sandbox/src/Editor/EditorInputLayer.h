#pragma once

#include <CHEngine/Layer/Layer.h>

namespace Sandbox { struct EditorContext; }

/// Слой ввода и навигации редактора.
///
/// Единственное место, где сырые события превращаются в действия редактора:
/// orbit-камера и обработка событий gizmo. Манипуляция gizmo и mouse-picking,
/// завязанные на ImGui-rect вьюпорта, остаются в UI-слое (внутри ImGui-прохода).
class EditorInputLayer : public CHEngine::Layer
{
public:
    explicit EditorInputLayer(Sandbox::EditorContext& ctx);
    void OnEvent(CHEngine::Event& e) override;

private:
    Sandbox::EditorContext& m_Ctx;
};
