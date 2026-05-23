#pragma once

#include <Core.h>
#include "CheStl/MemoryTypes.h"

#include "DeferredOps.h"
#include "EventBus.h"
#include "SystemScheduler.h"
#include "WorldsList.h"
#include "Physics/IPhysicsWorld.h"
#include "CHEngine/Scene/Components.h"

#include <string>
#include <unordered_map>

namespace CHEngine
{
    class Event;
    class EditorCamera;
    enum class WorldState : uint8_t
    {
        NONE,
        Presenting,
        Simulating,
        SimulatingWithoutPresenting
    };

    // You may not turn this into god class, so less functionality = good
    // It is just provides connection between Scene, Systems, Events and PhysicalWorld(if exists)
    class CHENGINE_API World {
    public:
        World(WorldsList* list);
        explicit World(Ref<Scene> scene, WorldsList* list);

        virtual ~World();

        void SetScene(Ref<Scene> scene);
        bool HasScene() const { return m_Scene != nullptr; }
        Ref<Scene> GetSceneRef() { return m_Scene; }

        void Update(Timestep dt);
        void OnEvent(Event& event);

        // Re-upload object transform UBOs from current TransformComponents.
        // Call after late-frame transform updates (gizmo drag) and before EndFrame.
        void RefreshRenderTransforms();

        // ── Play / Pause / Edit mode control ─────────────────────────────────
        void SetState(WorldState new_state);
        WorldState GetState() const { return m_State; }

        // WARNING !!!!!!!!!!!!!!!!
        // SHIT CODE but its necessary
        void SetActiveCamera(EditorCamera* camera) { m_Camera = camera; } // Set nullptr if you use camera from component
        EditorCamera* GetActiveCamera() { return m_Camera; }
        // ---------------------------

        void SetPhysicsWorldDesc(const PhysicsWorldDesc& world_desc);
        const PhysicsWorldDesc& GetPhysicsWorldDesc() const { return m_PhysicsWorldDesc; }
        Scope<IPhysicsWorld>& GetPhysicsRuntimeWorld() { return m_PhysicsWorld; }
        const Scope<IPhysicsWorld>& GetPhysicsRuntimeWorld() const { return m_PhysicsWorld; }

        // Getters
        SystemScheduler& GetScheduler() { return m_Scheduler; }
        const SystemScheduler& GetScheduler() const { return m_Scheduler; }
        DeferredOps& GetDeferredOps() { return m_DeferredOps; }
        const DeferredOps& GetDeferredOps() const { return m_DeferredOps; }
        EventBus& GetEvents() { return m_EventBus; }
        const EventBus& GetEvents() const { return m_EventBus; }

        WorldsList* GetWorlds() const { return m_Worlds; }
        const std::string& GetWorldName() const { return m_Name; }
        // Update the name by which this World is looked up in Worlds::TryGet.
        // Call after scene load to bind the World to the scene's file path.
        void SetWorldName(std::string name) { m_Name = std::move(name); }

    private:
        void RegisterDefaultSystems();
        void ApplyStateTransition(WorldState new_state);

        SystemScheduler  m_Scheduler;
        class RenderSystem* m_RenderSystem = nullptr; // cached pointer, owned by m_Scheduler
        DeferredOps m_DeferredOps;
        EventBus m_EventBus;
        PhysicsWorldDesc m_PhysicsWorldDesc{};
        Scope<IPhysicsWorld> m_PhysicsWorld;
        Ref<Scene> m_Scene      = nullptr;
        // WARNING !!!!!!!!!!!!!!!!
        // SHIT CODE but its necessary
        EditorCamera* m_Camera;
        // ---------------
        WorldState m_State = WorldState::NONE;
        WorldState m_PendingState = WorldState::NONE;

        WorldsList* m_Worlds = nullptr;
        std::string m_Name = "Untitled";
    };
}
