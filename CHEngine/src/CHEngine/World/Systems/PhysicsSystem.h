#pragma once

#include "CHEngine/Scene/Components.h"
#include "CHEngine/Scene/Scene.h"
#include "CHEngine/World/ISystem.h"
#include "Physics/IPhysicsWorld.h"

#include <cstdint>

namespace CHEngine {

    class ComponentRegistrationSystem;

class PhysicsSystem final : public ISystem {
public:
    explicit PhysicsSystem(uint8_t priority = 100)
        : ISystem(SystemPhase::Simulation, priority)
    {
    }
    ~PhysicsSystem() override = default;

    const char* GetName() const override { return "PhysicsSystem"; }
    void Run(World& world, DeferredOps& deferred_ops, Timestep dt) override;
    void OnBegin(World& world, DeferredOps& deferred_ops) override;
    void OnEnd(World& world, DeferredOps& deferred_ops) override;
    virtual void OnPhaseDispatch(World& world, DeferredOps& deferred_ops) override;

private:
    friend ComponentRegistrationSystem;
    friend World;

    static void RebuildPhysicsRuntime(World& world);
    static void ClearPhysicsRuntime(World& world);

    static void DestroyRigidBody(World& world, EntityHandle handle);
    static void CreateRigidBody(World& world, EntityHandle handle);
    static void OnRigidBodyComponentAdded(World& world, EntityHandle handle);
    static void OnRigidBodyComponentRemoved(World& world, EntityHandle handle);

    static bool ShouldWriteToPhysics(RigidBodySyncMode syncMode, PhysicsBodyType bodyType);
    static bool ShouldReadFromPhysics(RigidBodySyncMode syncMode, PhysicsBodyType bodyType);

    uint64_t m_RigidBodyAddedHookToken = 0;
    uint64_t m_RigidBodyRemovedHookToken = 0;
};

} // namespace CHEngine
