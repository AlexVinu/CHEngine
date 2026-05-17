#include "chepch.h"
#include "PhysicsSubsystem.h"

namespace CHEngine
{
    PhysicsSubsystem::PhysicsSubsystem(IPhysicsFactory* factory)
        : m_Factory(factory)
    {
        CHE_CORE_ASSERT(factory, "PhysicsSubsystem: factory must be non-null");
    }

    IPhysicsWorld* PhysicsSubsystem::CreateWorld(const PhysicsWorldDesc& worldDesc)
    {
        return m_Factory->CreateWorld(worldDesc);
    }

    void PhysicsSubsystem::DestroyWorld(IPhysicsWorld* world)
    {
        if (world) m_Factory->Delete(world);
    }

    IPhysicsShape* PhysicsSubsystem::CreateShape(const PhysicsColliderShapeDesc& shapeDesc)
    {
        switch (shapeDesc.Type)
        {
        case PhysicsColliderShapeType::Box:
            return m_Factory->CreateBoxShape(shapeDesc.HalfExtents);
        case PhysicsColliderShapeType::Sphere:
            return m_Factory->CreateSphereShape(shapeDesc.Radius);
        case PhysicsColliderShapeType::Capsule:
            return m_Factory->CreateCapsuleShape(shapeDesc.Radius, shapeDesc.HalfHeight);
        default:
            return nullptr;
        }
    }

    void PhysicsSubsystem::Delete(IPhysicsShape* shape)
    {
        if (shape) m_Factory->Delete(shape);
    }
}
