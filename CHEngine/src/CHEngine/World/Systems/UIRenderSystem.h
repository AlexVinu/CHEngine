#pragma once
#include "CHEngine/World/ISystem.h"

#include <glm/glm.hpp>

namespace CHEngine {

class World;
class DeferredOps;
class Timestep;
class Entity;
struct UIOverlayCanvasComponent;
struct UIWorldCanvasComponent;
struct UIRectTransformComponent;
class UIRendererBackend;

// Renders UI in two passes: screen-space overlay canvases and world-space canvases.
// UI-элементы — отдельные entity с UIRectTransformComponent, ссылающиеся на канвас по UUID.
// Runs in the Presentation phase after RenderSystem (priority 200).
class UIRenderSystem : public ISystem
{
public:
    UIRenderSystem()
        : ISystem(SystemPhase::Presentation, 200)
    {}

    const char* GetName() const override { return "UIRenderSystem"; }

    void Run(World& world, DeferredOps& deferred, Timestep ts) override;

private:
    struct UIRect { float x, y, w, h; };

    // Layout child rect inside parent rect (in parent's canvas-pixel coords).
    static UIRect ResolveRect(const UIRectTransformComponent& rt,
                              float parentW, float parentH);

    void DrawElement(UIRendererBackend* backend,
                     Entity* entity,
                     const UIRect& rect,
                     float canvasAlpha);
};

} // namespace CHEngine
