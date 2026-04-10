#pragma once

#include "CommandBuffer.h"
#include "SystemScheduler.h"
#include "Physics/IPhysicsWorld.h"
#include "CHEngine/Scene/Components.h"

#include <unordered_map>
#include <boost/container_hash/hash.hpp>

namespace CHEngine
{
    class CHENGINE_API World {
    public:
        World();
        explicit World(Scene* scene);
        World(const World&) = delete;
        World& operator=(const World&) = delete;
        World(World&&) noexcept = default;
        World& operator=(World&&) noexcept = default;

        void setScene(Scene* scene);
        bool hasScene() const { return m_Scene != nullptr; }
        Scene& scene();
        const Scene& scene() const;

        void update(Timestep dt);
        void OnEvent(Event& event);

        // ── Play / Pause / Edit mode control ─────────────────────────────────
        void setSimulating(bool sim) { m_Simulating = sim; }
        bool isSimulating()   const  { return m_Simulating; }
        void setActive(bool active)  { m_Active = active; }
        bool isActive()       const  { return m_Active; }

        void setPhysicsWorldDesc(const PhysicsWorldDesc& worldDesc);
        const PhysicsWorldDesc& physicsWorldDesc() const { return m_PhysicsWorldDesc; }
        void RebuildPhysicsRuntime();
        void ClearPhysicsRuntime();
        void DestroyRigidBodyRuntime(EntityHandle handle);
        Scope<IPhysicsWorld>& physicsRuntimeWorld() { return m_PhysicsWorld; }
        const Scope<IPhysicsWorld>& physicsRuntimeWorld() const { return m_PhysicsWorld; }

        SystemScheduler& scheduler() { return m_Scheduler; }
        const SystemScheduler& scheduler() const { return m_Scheduler; }
        CommandBuffer& commands() { return m_CommandBuffer; }
        const CommandBuffer& commands() const { return m_CommandBuffer; }

    private:
        void registerDefaultSystems();

    private:
        SystemScheduler m_Scheduler;
        CommandBuffer m_CommandBuffer;
        PhysicsWorldDesc m_PhysicsWorldDesc{};
        Scope<IPhysicsWorld> m_PhysicsWorld;
        Scene* m_Scene      = nullptr;
        bool   m_Active     = true;   // Presentation phase (рендер)
        bool   m_Simulating = false;  // Simulation phase (физика, логика)
    };
}