#pragma once

#include <Core.h>
#include "CheStl/MemoryTypes.h"

#include "DeferredOps.h"
#include "EventBus.h"
#include "SystemScheduler.h"
#include "Physics/IPhysicsWorld.h"
#include "CHEngine/Scene/Components.h"

#include <unordered_map>
#include <boost/container_hash/hash.hpp>

namespace CHEngine
{
    class Event;
    class EditorCamera;

    // You may not turn this into god class, so less functionality = good
    // It is just provides connection between Scene, Systems, Events and PhysicalWorld(if exists)
    class CHENGINE_API World {
    public:
        World();
        explicit World(Scene* scene);

        void SetScene(Scene* scene);
        bool HasScene() const { return m_Scene != nullptr; }
        Scene* GetScene();
        const Scene* GetScene() const;

        void Update(Timestep dt);
        void OnEvent(Event& event);

        // ── Play / Pause / Edit mode control ─────────────────────────────────
        void SetSimulating(bool sim) { m_Simulating = sim; }
        bool IsSimulating()   const  { return m_Simulating; }
        void SetActive(bool active)  { m_Active = active; }
        bool IsActive()       const  { return m_Active; }

        // WARNING !!!!!!!!!!!!!!!!
        // SHIT CODE but its necessary
        void SetActiveCamera(EditorCamera* camera) { m_Camera = camera; } // Set nullptr if you use camera from component
        EditorCamera* GetActiveCamera() { return m_Camera; }
        // ---------------------------

        void SetPhysicsWorldDesc(const PhysicsWorldDesc& world_desc);
        const PhysicsWorldDesc& GetPhysicsWorldDesc() const { return m_PhysicsWorldDesc; }
        Scope<IPhysicsWorld>& GetPhysicsRuntimeWorld() { return m_PhysicsWorld; }
        const Scope<IPhysicsWorld>& GetPhysicsRuntimeWorld() const { return m_PhysicsWorld; }

        SystemScheduler& GetScheduler() { return m_Scheduler; }
        const SystemScheduler& GetScheduler() const { return m_Scheduler; }
        DeferredOps& GetDeferredOps() { return m_DeferredOps; }
        const DeferredOps& GetDeferredOps() const { return m_DeferredOps; }
        EventBus& GetEvents() { return m_EventBus; }
        const EventBus& GetEvents() const { return m_EventBus; }

    private:
        void RegisterDefaultSystems();

    private:
        SystemScheduler m_Scheduler;
        DeferredOps m_DeferredOps;
        EventBus m_EventBus;
        PhysicsWorldDesc m_PhysicsWorldDesc{};
        Scope<IPhysicsWorld> m_PhysicsWorld;
        Scene* m_Scene      = nullptr;
        // WARNING !!!!!!!!!!!!!!!!
        // SHIT CODE but its necessary
        EditorCamera* m_Camera;
        // ---------------
        bool   m_Active     = true;   // Presentation phase (рендер)
        bool   m_Simulating = false;  // Simulation phase (физика, логика)
        bool   m_InitializationDispatched = false;
    };
}
