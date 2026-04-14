#pragma once

#include "CHEngine/World/ISystem.h"

namespace CHEngine {

class LifetimeSystem final : public ISystem {
public:
    explicit LifetimeSystem(uint8_t priority = 20)
        : ISystem(SystemPhase::Simulation, priority) {}

    const char* GetName() const override { return "LifetimeSystem"; }
    void Run(World& world, DeferredOps& deferred_ops, Timestep dt) override;
};

} // namespace CHEngine
