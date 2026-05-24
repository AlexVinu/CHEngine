#pragma once

#include "PhysicsTypes.h"

namespace CHEngine
{
    class IPhysicsContactListener
    {
    public:
        virtual ~IPhysicsContactListener() = default;
        virtual void OnContact(const PhysicsContactEvent& eventData) = 0;
    };
}
