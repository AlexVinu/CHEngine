#include "chepch.h"

#include "PhysicsFacade.h"

namespace
{
    CHEngine::IPhysicsFactory* g_PhysicsFactory = nullptr;

    CHEngine::IPhysicsShape* CreateShape(CHEngine::IPhysicsFactory* factory,
                                         const CHEngine::PhysicsColliderShapeDesc& shapeDesc)
    {
        if (!factory)
            return nullptr;

        switch (shapeDesc.Type)
        {
        case CHEngine::PhysicsColliderShapeType::Box:
            return factory->CreateBoxShape(shapeDesc.HalfExtents);
        case CHEngine::PhysicsColliderShapeType::Sphere:
            return factory->CreateSphereShape(shapeDesc.Radius);
        case CHEngine::PhysicsColliderShapeType::Capsule:
            return factory->CreateCapsuleShape(shapeDesc.Radius, shapeDesc.HalfHeight);
        default:
            return nullptr;
        }
    }
}

namespace CHEngine
{
    bool PhysicsFacade::Init(IPhysicsFactory* physicsFactory)
    {
        Shutdown();
        if (!physicsFactory)
            return false;

        g_PhysicsFactory = physicsFactory;
        return true;
    }

    void PhysicsFacade::Shutdown()
    {
        g_PhysicsFactory = nullptr;
    }

    bool PhysicsFacade::IsAvailable()
    {
        return g_PhysicsFactory != nullptr;
    }

    IPhysicsFactory* PhysicsFacade::GetFactory()
    {
        return g_PhysicsFactory;
    }

    IPhysicsWorld* PhysicsFacade::CreateWorld(const PhysicsWorldDesc& worldDesc)
    {
        if (!g_PhysicsFactory)
            return nullptr;
        return g_PhysicsFactory->CreateWorld(worldDesc);
    }

    void PhysicsFacade::DestroyWorld(IPhysicsWorld*& world)
    {
        if (!g_PhysicsFactory || !world)
            return;

        g_PhysicsFactory->Delete(world);
        world = nullptr;
    }

    bool PhysicsFacade::CreateRigidBodyRuntime(IPhysicsWorld* world,
                                               RigidBody3DComponent& rigidBody,
                                               const PhysicsTransform& initialTransform)
    {
        if (!g_PhysicsFactory || !world)
            return false;

        DestroyRigidBodyRuntime(world, rigidBody);

        IPhysicsShape* shape = CreateShape(g_PhysicsFactory, rigidBody.ShapeDesc);
        if (!shape)
            return false;

        PhysicsRigidBodyDesc bodyDesc = rigidBody.BodyDesc;
        bodyDesc.Transform = initialTransform;

        IPhysicsBody* body = world->CreateRigidBody(bodyDesc, shape);
        if (!body)
        {
            g_PhysicsFactory->Delete(shape);
            return false;
        }

        rigidBody.Shape = shape;
        rigidBody.Body = body;
        return true;
    }

    void PhysicsFacade::DestroyRigidBodyRuntime(IPhysicsWorld* world, RigidBody3DComponent& rigidBody)
    {
        if (world && rigidBody.Body)
            world->DestroyRigidBody(rigidBody.Body);
        rigidBody.Body = nullptr;

        if (g_PhysicsFactory && rigidBody.Shape)
            g_PhysicsFactory->Delete(rigidBody.Shape);
        rigidBody.Shape = nullptr;
    }
}
