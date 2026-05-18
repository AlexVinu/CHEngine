#pragma once

#include "CHEngine/World/ISystem.h"
#include "CHEngine/Scene/Scene.h"

namespace CHEngine {

// ─────────────────────────────────────────────────────────────────────────────
// UIInputSystem — обработка ввода для overlay UI элементов.
//
// Hit-test mouse-vs-button/slider на видимых overlay-канвасах. При события
// вызывает Lua-функции на скриптах сущности через convention:
//   Button:  function OnClick(entity)
//   Slider:  function OnChange(entity, value)   -- live во время drag
//            function OnRelease(entity, value)  -- по отпусканию
//
// World canvas в v1 не обрабатывается (требует raycast).
// Состояние ввода используется из Input:: (window-coords).
// ─────────────────────────────────────────────────────────────────────────────
class UIInputSystem final : public ISystem
{
public:
    explicit UIInputSystem(uint8_t priority = 6);
    ~UIInputSystem() override = default;

    const char* GetName() const override { return "UIInputSystem"; }

    void Run(World& world, DeferredOps& deferred, Timestep dt) override;

private:
    EntityHandle m_PressedButton{};
    EntityHandle m_ActiveSlider{};
};

} // namespace CHEngine
