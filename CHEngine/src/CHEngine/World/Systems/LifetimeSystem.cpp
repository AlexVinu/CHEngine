#include "chepch.h"
#include "LifetimeSystem.h"

#include "CHEngine/World/World.h"
#include "CHEngine/World/WorldEvents.h"

namespace CHEngine {

void LifetimeSystem::Run(World& world, DeferredOps& deferred_ops, Timestep dt)
{
    if (dt <= 0.0f)
        return;

    auto scene = world.GetSceneRef();
    scene->ForEach<LifetimeComponent>([&](EntityHandle handle, const UUID&, LifetimeComponent& lifetime) {
        if (!lifetime.DestroyOnExpire || lifetime.RemainingSeconds <= 0.0f)
            return;

        lifetime.RemainingSeconds -= dt;
        if (lifetime.RemainingSeconds <= 0.0f)
        {
            world.GetEvents().Publish<EntityExpiredEvent>(SystemPhase::Simulation, EntityExpiredEvent{ handle });
            deferred_ops.DestroyEntity(handle);
        }
    });
}

} // namespace CHEngine
