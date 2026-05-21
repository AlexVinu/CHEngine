#include "chepch.h"
#include "ComponentMeta.h"
#include "Components.h"

#include <entt/entt.hpp>

namespace CHEngine::Meta {

using namespace entt::literals;

namespace {

// Default clone: copies the component via copy-constructor.
template<typename T>
void CloneTo(entt::registry& dst, entt::entity dstE,
             entt::registry& src, entt::entity srcE)
{
    if (!src.all_of<T>(srcE)) return;
    dst.emplace_or_replace<T>(dstE, src.get<T>(srcE));
}

// RigidBody3DComponent: copy only the data desc, leave runtime pointers null.
// IPhysicsBody* and IPhysicsShape* will be (re)created via OnTransferIn / OnComponentAdded hooks.
template<>
void CloneTo<RigidBody3DComponent>(entt::registry& dst, entt::entity dstE,
                                    entt::registry& src, entt::entity srcE)
{
    if (!src.all_of<RigidBody3DComponent>(srcE)) return;
    const auto& s = src.get<RigidBody3DComponent>(srcE);
    auto& d = dst.emplace_or_replace<RigidBody3DComponent>(dstE);
    d.BodyDesc             = s.BodyDesc;
    d.ShapeDesc            = s.ShapeDesc;
    d.SyncMode             = s.SyncMode;
    d.SynchronisedTransform = s.SynchronisedTransform;
    d.Body                 = nullptr;
    d.Shape                = nullptr;
}

template<typename... Ts>
void RegisterGroup(ComponentGroup<Ts...>)
{
    (entt::meta<Ts>()
        .template func<&CloneTo<Ts>>("clone_to"_hs), ...);
}

} // namespace

void RegisterAllComponents()
{
    RegisterGroup(CopyableSceneComponents{});
}

} // namespace CHEngine::Meta
