#pragma once

#include "CHEngine/Scene/Components.h"
#include "CHEngine/Scene/Scene.h"
#include "CHEngine/World/ISystem.h"
#include "Physics/IPhysicsWorld.h"

namespace CHEngine {

class PhysicsSystem final : public ISystem {
public:
    explicit PhysicsSystem(uint8_t priority = 100)
        : ISystem(SystemPhase::Simulation, priority)
    {
    }
    ~PhysicsSystem() override = default;

    const char* name() const override { return "PhysicsSystem"; }
    void run(World& world, CommandBuffer& commands, Timestep dt) override;

private:
    static bool ShouldWriteToPhysics(RigidBodySyncMode syncMode, PhysicsBodyType bodyType);
    static bool ShouldReadFromPhysics(RigidBodySyncMode syncMode, PhysicsBodyType bodyType);
};

} // namespace CHEngine
