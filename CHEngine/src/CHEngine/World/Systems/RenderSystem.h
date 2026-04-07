#pragma once

#include "CHEngine/World/ISystem.h"

namespace CHEngine {

class RenderSystem final : public ISystem
{
public:
    explicit RenderSystem(uint8_t priority = 100)
        : ISystem(SystemPhase::Presentation, priority) {}

    const char* name() const override { return "RenderSystem"; }
    void run(World& world, CommandBuffer& commands, Timestep dt) override;
};

} // namespace CHEngine
