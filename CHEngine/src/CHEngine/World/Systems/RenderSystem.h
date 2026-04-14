#pragma once

#include "CHEngine/World/ISystem.h"

namespace CHEngine {

class RenderSystem final : public ISystem
{
public:
    explicit RenderSystem(uint8_t priority = 100)
        : ISystem(SystemPhase::Presentation, priority) {}

    const char* GetName() const override { return "RenderSystem"; }
    void Run(World& world, DeferredOps& deferred_ops, Timestep dt) override;
};

} // namespace CHEngine
