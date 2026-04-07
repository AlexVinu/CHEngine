#pragma once

#include "CHEngine/World/ISystem.h"

namespace CHEngine {

class LifetimeSystem final : public ISystem {
public:
    explicit LifetimeSystem(uint8_t priority = 20)
        : ISystem(SystemPhase::Simulation, priority) {}

    const char* name() const override { return "LifetimeSystem"; }
    void run(World& world, CommandBuffer& commands, Timestep dt) override;
};

} // namespace CHEngine
