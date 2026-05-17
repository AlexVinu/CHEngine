#include "chepch.h"
#include "World.h"

#include "Systems/ComponentValidationSystem.h"
#include "Systems/LifetimeSystem.h"
#include "Systems/LuaScriptSystem.h"
#include "Systems/PhysicsSystem.h"
#include "Systems/RenderSystem.h"
#include "Systems/UIRenderSystem.h"
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

    World::World(Ref<Scene> scene)
        : m_Scene(scene)
        , m_Camera(nullptr)
    {
        RegisterDefaultSystems();
    }

	World::~World()
	{
        ApplyStateTransition(WorldState::NONE);
	}

	void World::SetScene(Ref<Scene> scene)
    {
        if (m_Scene == scene)
            return;

        CHE_CORE_INFO("World::SetScene rebind requested (hasScene={})", scene != nullptr);
        m_Scene = scene;
    }

    void World::SetState(WorldState new_state)
    {
        m_PendingState = new_state;
    }

    void World::SetPhysicsWorldDesc(const PhysicsWorldDesc& world_desc)
    {
        m_PhysicsWorldDesc = world_desc;
    }

    void World::Update(Timestep dt)
    {
        if (!m_Scene)
            return;

        // Processing state changes BEFORE everything else
        if (m_PendingState != m_State)
            ApplyStateTransition(m_PendingState);

        if (m_State == WorldState::Simulating || m_State == WorldState::SimulatingWithoutPresenting)
            m_Scheduler.RunPhase(SystemPhase::Simulation, *this, m_DeferredOps, dt);

        if (m_State == WorldState::Presenting || m_State == WorldState::Simulating)
            m_Scheduler.RunPhase(SystemPhase::Presentation, *this, m_DeferredOps, dt);

        // Processing deffered operations like components changes/creations/deletions
        m_DeferredOps.Flush(*this, m_Scene);
    }

    void World::ApplyStateTransition(WorldState new_state)
{
    if (new_state == m_State)
        return;

    // Turn off the world
    if (new_state == WorldState::NONE)
    {
		m_Scheduler.NotifyEnd(SystemPhase::Presentation, *this, m_DeferredOps);
		m_Scheduler.NotifyEnd(SystemPhase::Simulation, *this, m_DeferredOps);
        m_State = WorldState::NONE;
        return;
    }

    // --- EXIT current state ---
    switch (m_State)
    {
        case WorldState::Simulating:
            m_Scheduler.NotifyEnd(SystemPhase::Simulation, *this, m_DeferredOps);
            m_Scheduler.NotifyEnd(SystemPhase::Presentation, *this, m_DeferredOps);
            break;
        case WorldState::Presenting:
            m_Scheduler.NotifyEnd(SystemPhase::Presentation, *this, m_DeferredOps);
            break;
        case WorldState::SimulatingWithoutPresenting:
            m_Scheduler.NotifyEnd(SystemPhase::Simulation, *this, m_DeferredOps);
            break;
        default:
            break;
    }

    m_State = new_state;

    // --- ENTER new state ---
    switch (m_State)
    {
        case WorldState::Simulating:
            m_Scheduler.NotifyBegin(SystemPhase::Presentation, *this, m_DeferredOps);
            m_Scheduler.NotifyBegin(SystemPhase::Simulation, *this, m_DeferredOps);
            break;
        case WorldState::Presenting:
            m_Scheduler.NotifyBegin(SystemPhase::Presentation, *this, m_DeferredOps);
            break;
        case WorldState::SimulatingWithoutPresenting:
            m_Scheduler.NotifyBegin(SystemPhase::Simulation, *this, m_DeferredOps);
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

        // Lua scripts: пара систем с общим host'ом.
        // PreSim (priority 5) — OnUpdate до физики.
        // PostSim (priority 150) — OnLateUpdate после физики.
        {
            auto scriptHost = MakeScriptHost();
            m_Scheduler.EmplaceSystem<LuaPreSimSystem>(scriptHost);
            m_Scheduler.EmplaceSystem<LuaPostSimSystem>(scriptHost);
        }

        m_Scheduler.EmplaceSystem<PhysicsSystem>();
        m_RenderSystem = &m_Scheduler.EmplaceSystem<RenderSystem>();
        // UIRenderSystem is NOT in the scheduler — it must run during the ImGui phase
        // (GetForegroundDrawList only works between ImGui::NewFrame and EndFrame)
        // Called explicitly from SceneViewLayer_ImGuiFrame and PlayerLayer::OnImGuiRender
    }

    void World::RefreshRenderTransforms()
    {
        if (!m_RenderSystem || !m_Scene)
            return;
        m_RenderSystem->RefreshTransforms(*m_Scene);
    }
}
