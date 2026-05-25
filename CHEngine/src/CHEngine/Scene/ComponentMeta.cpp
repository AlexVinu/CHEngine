#include "chepch.h"
#include "ComponentMeta.h"
#include "Components.h"
#include "ComponentMeta.h"
#include "MetaSerializer.h"


#include <entt/entt.hpp>

namespace CHEngine {

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

// RigidBody3DComponent: copy only the data desc, leave runtime handles invalid.
// PhysBody / PhysShape handles will be (re)created via OnTransferIn / OnComponentAdded hooks.
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
    d.Body                 = {};
    d.Shape                = {};
}

template<typename T>
void RegisterComponentMeta()
{
	auto factory = entt::meta<T>();
	factory.template func<&CloneTo<T>>("clone_to"_hs);
	factory.template func<&SerializeComponent<T>>("serialize"_hs);
	factory.template func<&DeserializeComponent<T>>("deserialize"_hs);
}

template<typename... Ts>
void RegisterGroup(ComponentGroup<Ts...>)
{
	(RegisterComponentMeta<Ts>(), ...);
}

} // namespace

void RegisterAllComponents()
{
	RegisterGroup(AllComponents{});
}

} // namespace CHEngine::Meta
