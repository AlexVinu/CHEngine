#include "chepch.h"
#include "World.h"

#include "Systems/ComponentValidationSystem.h"
#include "Systems/LifetimeSystem.h"
#include "Systems/LuaScriptSystem.h"
#include "Systems/UIInputSystem.h"
#include "Systems/PhysicsSystem.h"
#include "Systems/RenderSystem.h"
#include "Systems/UIRenderSystem.h"
#include "CHEngine/Scene/Entity.h"
#include "CHEngine/Scene/SceneSerializer.h"
#include "CHEngine/Utils/AppPaths.h"
#include "WorldEvents.h"

#include <glm/gtc/quaternion.hpp>
#include <filesystem>
#include <vector>

namespace CHEngine
{
    World::World(WorldsList* list)
        : m_Scene(nullptr)
        , m_Camera(nullptr),
        m_Worlds(list)
    {
        RegisterDefaultSystems();
    }

    World::World(Ref<Scene> scene, WorldsList* list)
		: m_Scene(scene)
		, m_Camera(nullptr)
		, m_Worlds(list)
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

    void World::Simulate(Timestep dt)
    {
        if (!m_Scene)
            return;

        // Processing state changes BEFORE everything else
        if (m_PendingState != m_State)
            ApplyStateTransition(m_PendingState);

        if (m_State == WorldState::Simulating || m_State == WorldState::SimulatingWithoutPresenting)
            m_Scheduler.RunPhase(SystemPhase::Simulation, *this, m_DeferredOps, dt);
    }

    void World::Render(Timestep dt)
    {
        if (!m_Scene)
            return;

        if (m_State == WorldState::Presenting || m_State == WorldState::Simulating)
            m_Scheduler.RunPhase(SystemPhase::Presentation, *this, m_DeferredOps, dt);
    }

    void World::PostUpdate()
    {
        if (!m_Scene)
            return;

        // Processing deffered operations like components changes/creations/deletions
        m_DeferredOps.Flush(*this, m_Scene);

        // Scene switch happens last — outside every system's ForEach.
        if (m_HasPendingSceneLoad)
            ProcessPendingSceneLoad();
    }

    void World::RequestSceneLoad(const std::string& relPath)
    {
        m_PendingSceneLoad    = relPath;
        m_HasPendingSceneLoad = true;
    }

    void World::ProcessPendingSceneLoad()
    {
        m_HasPendingSceneLoad = false;
        const std::string relPath = std::move(m_PendingSceneLoad);
        m_PendingSceneLoad.clear();

        const std::filesystem::path exeDir = AppPaths::ExecutableDir();
        const std::filesystem::path full   = exeDir / relPath;

        SceneSerializer serializer;
        Ref<Scene> next = serializer.LoadFromFile(full.string(), exeDir);
        if (!next)
        {
            CHE_CORE_ERROR("World::RequestSceneLoad: failed to load scene '{}'", relPath.c_str());
            return;
        }

        // Restart running systems around the swap so OnBegin runs for the new scene.
        const WorldState prev = m_State;
        ApplyStateTransition(WorldState::NONE);
        m_Scene = next;
        SetWorldName(relPath);
        m_PendingState = prev;
        ApplyStateTransition(prev);

        CHE_CORE_INFO("World: switched to scene '{}'", relPath.c_str());
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

        m_Scheduler.EmplaceSystem<LuaScriptSystem>();
        m_Scheduler.EmplaceSystem<UIInputSystem>();

        m_Scheduler.EmplaceSystem<PhysicsSystem>();
        m_RenderSystem = &m_Scheduler.EmplaceSystem<RenderSystem>();
        m_Scheduler.EmplaceSystem<UIRenderSystem>();
    }

    void World::RefreshRenderTransforms()
    {
        if (!m_RenderSystem || !m_Scene)
            return;
        m_RenderSystem->RefreshTransforms(*m_Scene);
    }
}
