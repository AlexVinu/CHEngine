#pragma once

#include "CHEngine/Scene/Components.h"
#include "CHEngine/Scene/Scene.h"
#include "CHEngine/World/ISystem.h"
#include "Physics/Handles.h"
#include "Physics/PhysicsTypes.h"
#include "CHEngine/World/DeferredOps.h"

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

    static bool ShouldWriteToPhysics(RigidBodySyncMode syncMode, PhysicsBodyType bodyType);
    static bool ShouldReadFromPhysics(RigidBodySyncMode syncMode, PhysicsBodyType bodyType);

    static void OnTransferOut(World& world, EntityHandle handle, TransferContext& ctx);
    static void OnTransferIn(World& world, EntityHandle handle, const TransferContext& ctx);

    // Tokens for deferred ops
	DeferredOps::HookHandle m_RigidBodyAddedHookHandle;
	DeferredOps::HookHandle m_RigidBodyRemovedHookHandle;
    DeferredOps::HookHandle m_RigidBodyTransferOutHookHandle;
    DeferredOps::HookHandle m_RigidBodyTransferInHookHandle;
};

} // namespace CHEngine
