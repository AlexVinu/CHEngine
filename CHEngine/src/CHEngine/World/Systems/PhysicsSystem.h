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

    const char* GetName() const override { return "PhysicsSystem"; }
    void Run(World& world, DeferredOps& deferred_ops, Timestep dt) override;
    void OnPhaseDispatch(World& world, DeferredOps& deferred_ops) override;

    static void RebuildPhysicsRuntime(World& world);
    static void ClearPhysicsRuntime(World& world);
    static void DestroyRigidBody(World& world, EntityHandle handle);

    static bool ShouldWriteToPhysics(RigidBodySyncMode syncMode, PhysicsBodyType bodyType);
    static bool ShouldReadFromPhysics(RigidBodySyncMode syncMode, PhysicsBodyType bodyType);
};

} // namespace CHEngine
