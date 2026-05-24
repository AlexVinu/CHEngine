#include "chepch.h"
#include "PhysicsSubsystem.h"

namespace CHEngine
{
    PhysicsSubsystem::PhysicsSubsystem(IPhysicsFactory* factory)
        : m_Factory(factory)
    {
        CHE_CORE_ASSERT(factory, "PhysicsSubsystem: factory must be non-null");
    }

    PhysWorldHandle PhysicsSubsystem::CreateWorld(const PhysicsWorldDesc& worldDesc)
    {
        return m_Factory->CreateWorld(worldDesc);
    }

    void PhysicsSubsystem::DestroyWorld(PhysWorldHandle world)
    {
        if (world.IsValid()) m_Factory->Delete(world);
    }

    PhysShapeHandle PhysicsSubsystem::CreateShape(const PhysicsColliderShapeDesc& shapeDesc)
    {
        // Конвертируем POD-desc → variant.
        switch (shapeDesc.Type)
        {
        case PhysShapeType::Box:
            return m_Factory->CreateShape(BoxDesc{ shapeDesc.HalfExtents });
        case PhysShapeType::Sphere:
            return m_Factory->CreateShape(SphereDesc{ shapeDesc.Radius });
        case PhysShapeType::Capsule:
            return m_Factory->CreateShape(CapsuleDesc{ shapeDesc.Radius, shapeDesc.HalfHeight });
        default:
            return {};
        }
    }

    void PhysicsSubsystem::Delete(PhysShapeHandle shape)
    {
        if (shape.IsValid()) m_Factory->Delete(shape);
    }
}
