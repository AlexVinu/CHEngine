#include "chepch.h"

#include "PhysicsFactoryPhysX.h"

#include "PhysicsWorldPhysX.h"
#include "ShapePhysX.h"

#include <extensions/PxExtensionsAPI.h>

namespace CHModules
{
    PhysicsFactoryPhysX::PhysicsFactoryPhysX()
    {
        m_Foundation = PxCreateFoundation(PX_PHYSICS_VERSION, m_Allocator, m_ErrorCallback);
        if (!m_Foundation)
        {
            CHE_CORE_ERROR("PhysicsFactoryPhysX: failed to create PxFoundation");
            return;
        }

        physx::PxTolerancesScale scale;
        m_Physics = PxCreatePhysics(PX_PHYSICS_VERSION, *m_Foundation, scale, false, nullptr);
        if (!m_Physics)
        {
            CHE_CORE_ERROR("PhysicsFactoryPhysX: failed to create PxPhysics");
            return;
        }

        if (!PxInitExtensions(*m_Physics, nullptr))
            CHE_CORE_ERROR("PhysicsFactoryPhysX: PxInitExtensions failed");
    }

    PhysicsFactoryPhysX::~PhysicsFactoryPhysX()
    {
        if (m_Physics)
        {
            PxCloseExtensions();
            m_Physics->release();
            m_Physics = nullptr;
        }
        if (m_Foundation)
        {
            m_Foundation->release();
            m_Foundation = nullptr;
        }
    }

    CHEngine::IPhysicsWorld* PhysicsFactoryPhysX::CreateWorld(const CHEngine::PhysicsWorldDesc& worldDesc)
    {
        if (!m_Physics) return nullptr;
        auto* world = CreateImpl<PhysicsWorldPhysX>(*m_Physics, worldDesc);
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
        return m_Foundation != nullptr && m_Physics != nullptr;
    }

    CHEngine::ModuleType PhysicsFactoryPhysX::GetType() const
    {
        return CHEngine::ModuleType::Physics;
    }
}

IMPLEMENT_MODULE_FACTORY(CHModules::PhysicsFactoryPhysX)
