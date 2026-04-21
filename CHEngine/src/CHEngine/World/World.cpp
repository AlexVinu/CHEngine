#include "chepch.h"
#include "World.h"

#include "Systems/ComponentValidationSystem.h"
#include "Systems/LifetimeSystem.h"
#include "Systems/PhysicsSystem.h"
#include "Systems/RenderSystem.h"
#include "CHEngine/Physics/PhysicsFacade.h"
#include "CHEngine/Scene/Entity.h"
#include "WorldEvents.h"

#include <glm/gtc/quaternion.hpp>
#include <vector>

namespace CHEngine
{
    World::World()
        : m_Scene(nullptr)
        , m_Camera(nullptr)
    {
        RegisterDefaultSystems();
    }

    World::World(Scene* scene)
        : m_Scene(scene)
        , m_Camera(nullptr)
    {
        RegisterDefaultSystems();
    }

    void World::SetScene(Scene* scene)
    {
        if (m_Scene == scene)
            return;

        CHE_CORE_INFO("World::SetScene rebind requested (hasScene={})", scene != nullptr);
        m_Scene = scene;
        m_DeferredOps.Clear();
        m_EventBus.ClearAll();
        m_State = WorldState::Idle;
        m_PendingState = WorldState::Idle;
        m_InitializationDispatched = false;
    }

    void World::SetState(WorldState new_state)
    {
        m_PendingState = new_state;
    }

    void World::SetPhysicsWorldDesc(const PhysicsWorldDesc& world_desc)
    {
        m_PhysicsWorldDesc = world_desc;
    }

    Scene* World::GetScene()
    {
        CHE_CORE_ASSERT(m_Scene, "World::GetScene called without a bound scene");
        return m_Scene;
    }

    const Scene* World::GetScene() const
    {
        CHE_CORE_ASSERT(m_Scene, "World::GetScene called without a bound scene");
        return m_Scene;
    }

    void World::Update(Timestep dt)
    {
        if (!m_Scene)
            return;

        if (m_PendingState != m_State)
            ApplyStateTransition(m_PendingState);

        if (m_State == WorldState::Simulating && !m_InitializationDispatched)
        {
            m_Scheduler.RunPhase(SystemPhase::Initialization, *this, m_DeferredOps, dt);
            m_InitializationDispatched = true;
        }

        if (m_State == WorldState::Simulating)
            m_Scheduler.RunPhase(SystemPhase::Simulation, *this, m_DeferredOps, dt);

        if (m_State == WorldState::Presenting || m_State == WorldState::Simulating)
            m_Scheduler.RunPhase(SystemPhase::Presentation, *this, m_DeferredOps, dt);

        m_DeferredOps.Flush(m_Scene);
    }

    void World::ApplyStateTransition(WorldState new_state)
{
    if (new_state == m_State)
        return;

    // --- EXIT текущего состояния ---
    switch (m_State)
    {
        case WorldState::Simulating:
            m_Scheduler.NotifyEnd(SystemPhase::Simulation, *this, m_DeferredOps);
            m_Scheduler.NotifyEnd(SystemPhase::Presentation, *this, m_DeferredOps);
            break;
        case WorldState::Presenting:
            m_Scheduler.NotifyEnd(SystemPhase::Presentation, *this, m_DeferredOps);
            break;
        default:
            break;
    }

    m_State = new_state;

    // --- ENTER нового состояния ---
    switch (m_State)
    {
        case WorldState::Simulating:
            m_Scheduler.NotifyBegin(SystemPhase::Presentation, *this, m_DeferredOps);
            m_Scheduler.NotifyBegin(SystemPhase::Simulation, *this, m_DeferredOps);
            m_InitializationDispatched = false;
            break;
        case WorldState::Presenting:
            m_Scheduler.NotifyBegin(SystemPhase::Presentation, *this, m_DeferredOps);
            break;
        default:
            break;
    }
}

    void World::OnEvent(Event& event)
    {
        (void)event;
    }

    void World::RegisterDefaultSystems()
    {
        m_Scheduler.EmplaceSystem<LifetimeSystem>();
        m_Scheduler.EmplaceSystem<ComponentValidationSystem>();
        m_Scheduler.EmplaceSystem<PhysicsSystem>();
        m_Scheduler.EmplaceSystem<RenderSystem>();
    }
}
