#pragma once

#include "CHEngine/World/ISystem.h"

namespace CHEngine {

class ComponentValidationSystem final : public ISystem {
public:
    explicit ComponentValidationSystem(uint8_t priority = 30)
        : ISystem(SystemPhase::Simulation, priority) {}

    const char* name() const override { return "ComponentValidationSystem"; }
    void run(World& world, CommandBuffer& commands, Timestep dt) override;
};

} // namespace CHEngine
