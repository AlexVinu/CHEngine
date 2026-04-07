#include "chepch.h"
#include "LifetimeSystem.h"

#include "CHEngine/World/CommandBuffer.h"
#include "CHEngine/World/World.h"

namespace CHEngine {

void LifetimeSystem::run(World& world, CommandBuffer& commands, Timestep dt)
{
    if (dt <= 0.0f)
        return;

    Scene& scene = world.scene();
    scene.ForEach<LifetimeComponent>([&](EntityHandle handle, const UUID&, LifetimeComponent& lifetime) {
        if (!lifetime.DestroyOnExpire || lifetime.RemainingSeconds <= 0.0f)
            return;

        lifetime.RemainingSeconds -= dt;
        if (lifetime.RemainingSeconds <= 0.0f)
            commands.destroyEntity(handle);
    });
}

} // namespace CHEngine
