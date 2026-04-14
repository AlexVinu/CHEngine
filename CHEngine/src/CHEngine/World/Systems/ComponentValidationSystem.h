#pragma once

#include "CHEngine/World/ISystem.h"

namespace CHEngine {

class ComponentValidationSystem final : public ISystem {
public:
    explicit ComponentValidationSystem(uint8_t priority = 30)
        : ISystem(SystemPhase::Simulation, priority) {}

    const char* GetName() const override { return "ComponentValidationSystem"; }
    void Run(World& world, DeferredOps& deferred_ops, Timestep dt) override;
};

} // namespace CHEngine
