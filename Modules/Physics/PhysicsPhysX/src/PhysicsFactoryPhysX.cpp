#include "chepch.h"

#include "PhysicsFactoryPhysX.h"

#include "PhysicsWorldPhysX.h"
#include "ShapePhysX.h"

namespace CHModules
{
    CHEngine::IPhysicsWorld* PhysicsFactoryPhysX::CreateWorld(const CHEngine::PhysicsWorldDesc& worldDesc)
    {
        auto* world = CreateImpl<PhysicsWorldPhysX>(worldDesc);
        if (!world->IsValid())
        {
            DestroyImpl(world);
            return nullptr;
        }
        return world;
    }

    CHEngine::IPhysicsShape* PhysicsFactoryPhysX::CreateBoxShape(const glm::vec3& halfExtents)
    {
        return CreateImpl<ShapePhysX>(halfExtents);
    }

    CHEngine::IPhysicsShape* PhysicsFactoryPhysX::CreateSphereShape(float radius)
    {
        return CreateImpl<ShapePhysX>(radius);
    }

    CHEngine::IPhysicsShape* PhysicsFactoryPhysX::CreateCapsuleShape(float radius, float halfHeight)
    {
        return CreateImpl<ShapePhysX>(radius, halfHeight);
    }

    void PhysicsFactoryPhysX::Delete(CHEngine::IPhysicsWorld* world)
    {
        if (!world) return;
        DestroyImpl(static_cast<PhysicsWorldPhysX*>(world));
    }

    void PhysicsFactoryPhysX::Delete(CHEngine::IPhysicsShape* shape)
    {
        if (!shape) return;
        DestroyImpl(static_cast<ShapePhysX*>(shape));
    }

    CHEngine::EPhysicsAPI PhysicsFactoryPhysX::GetPhysicsApi()
    {
        return CHEngine::EPhysicsAPI::PhysX;
    }

    bool PhysicsFactoryPhysX::CheckIsWorking()
    {
        CHEngine::PhysicsWorldDesc desc{};
        PhysicsWorldPhysX testWorld(desc);
        return testWorld.IsValid();
    }

    CHEngine::ModuleType PhysicsFactoryPhysX::GetType() const
    {
        return CHEngine::ModuleType::Physics;
    }
}

IMPLEMENT_MODULE_FACTORY(CHModules::PhysicsFactoryPhysX)
