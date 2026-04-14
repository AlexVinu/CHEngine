#pragma once

#include "CHEngine/Scene/Scene.h"

namespace CHEngine {

struct RebuildPhysicsWorldEvent
{
};

struct DestroyPhysicsWorldEvent
{
};

struct DestroyRigidBodyEvent
{
    EntityHandle entityHandle{};
};

struct EntityExpiredEvent {
    EntityHandle entityHandle{};
};

struct SaveWorldEvent {
};

} // namespace CHEngine

