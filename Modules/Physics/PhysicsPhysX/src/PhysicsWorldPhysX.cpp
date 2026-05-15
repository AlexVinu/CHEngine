#include "chepch.h"

#include "PhysicsWorldPhysX.h"

#include "BodyPhysX.h"
#include "ShapePhysX.h"

#include <extensions/PxExtensionsAPI.h>

#include <algorithm>

namespace
{
    physx::PxVec3 ToPxVec3(const glm::vec3& v)
    {
        return { v.x, v.y, v.z };
    }

    glm::vec3 FromPxVec3(const physx::PxVec3& v)
    {
        return { v.x, v.y, v.z };
    }

    physx::PxQuat ToPxQuat(const glm::quat& q)
    {
        return { q.x, q.y, q.z, q.w };
    }

    physx::PxTransform ToPxTransform(const glm::vec3& position, const glm::quat& rotation)
    {
        return { ToPxVec3(position), ToPxQuat(rotation) };
    }

    physx::PxTransform ToPxTransform(const CHEngine::PhysicsTransform& t)
    {
        return { ToPxVec3(t.Position), ToPxQuat(t.Rotation) };
    }

}

namespace CHModules
{
    inline PhysicsWorldPhysX::ContactCallbackPhysX::ContactCallbackPhysX(PhysicsWorldPhysX* owner)
        : m_Owner(owner)
    {
    }
    void PhysicsWorldPhysX::ContactCallbackPhysX::onConstraintBreak(physx::PxConstraintInfo*, physx::PxU32) {}
    void PhysicsWorldPhysX::ContactCallbackPhysX::onWake(physx::PxActor**, physx::PxU32) {}
    void PhysicsWorldPhysX::ContactCallbackPhysX::onSleep(physx::PxActor**, physx::PxU32)  {}
    void PhysicsWorldPhysX::ContactCallbackPhysX::onTrigger(physx::PxTriggerPair*, physx::PxU32) {}
    void PhysicsWorldPhysX::ContactCallbackPhysX::onAdvance(const physx::PxRigidBody* const*, const physx::PxTransform*, const physx::PxU32) {}

    void PhysicsWorldPhysX::ContactCallbackPhysX::onContact(const physx::PxContactPairHeader& pairHeader,
                    const physx::PxContactPair* pairs,
                    physx::PxU32 nbPairs)
    {
        if (!m_Owner || !m_Owner->m_ContactListener) return;

        BodyPhysX* bodyA = m_Owner->FindBodyByActor(static_cast<const physx::PxRigidActor*>(pairHeader.actors[0]));
        BodyPhysX* bodyB = m_Owner->FindBodyByActor(static_cast<const physx::PxRigidActor*>(pairHeader.actors[1]));

        if (!bodyA || !bodyB) return;

        for (physx::PxU32 i = 0; i < nbPairs; ++i)
        {
            physx::PxContactPairPoint contactPoints[8];
            const physx::PxU32 pointCount = pairs[i].extractContacts(contactPoints, 8);
            if (pointCount == 0) continue;

            CHEngine::PhysicsContactEvent eventData{};
            eventData.BodyA = bodyA;
            eventData.BodyB = bodyB;
            eventData.Point = FromPxVec3(contactPoints[0].position);
            eventData.Normal = FromPxVec3(contactPoints[0].normal);
            m_Owner->m_ContactListener->OnContact(eventData);
        }
    }

    PhysicsWorldPhysX::PhysicsWorldPhysX(const CHEngine::PhysicsWorldDesc& worldDesc)
    {
        m_Foundation = PxCreateFoundation(PX_PHYSICS_VERSION, m_Allocator, m_ErrorCallback);
        if (!m_Foundation)
        {
            CHE_CORE_ERROR("PhysicsWorldPhysX: failed to create PhysX foundation");
            return;
        }

        physx::PxTolerancesScale scale;
        m_Physics = PxCreatePhysics(PX_PHYSICS_VERSION, *m_Foundation, scale, false, nullptr);
        if (!m_Physics)
        {
            CHE_CORE_ERROR("PhysicsWorldPhysX: failed to create PhysX");
            return;
        }

        if (!PxInitExtensions(*m_Physics, nullptr))
        {
            CHE_CORE_ERROR("PhysicsWorldPhysX: PxInitExtensions failed");
            return;
        }

        m_Dispatcher = physx::PxDefaultCpuDispatcherCreate(std::max(1u, worldDesc.SolverThreads));
        if (!m_Dispatcher)
        {
            CHE_CORE_ERROR("PhysicsWorldPhysX: failed to create dispatcher");
            return;
        }

        physx::PxSceneDesc sceneDesc(scale);
        sceneDesc.gravity = ToPxVec3(worldDesc.Gravity);
        sceneDesc.cpuDispatcher = m_Dispatcher;
        sceneDesc.filterShader = physx::PxDefaultSimulationFilterShader;

        m_ContactCallback = std::make_unique<ContactCallbackPhysX>(this);
        sceneDesc.simulationEventCallback = m_ContactCallback.get();

        m_Scene = m_Physics->createScene(sceneDesc);
        if (!m_Scene)
        {
            CHE_CORE_ERROR("PhysicsWorldPhysX: failed to create scene");
            return;
        }
    }

    PhysicsWorldPhysX::~PhysicsWorldPhysX()
    {
        m_Bodies.clear();

        if (m_Scene)
        {
            m_Scene->release();
            m_Scene = nullptr;
        }
        if (m_Dispatcher)
        {
            m_Dispatcher->release();
            m_Dispatcher = nullptr;
        }
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

    void PhysicsWorldPhysX::SetGravity(const glm::vec3& gravity)
    {
        if (!m_Scene) return;
        m_Scene->setGravity(ToPxVec3(gravity));
    }

    void PhysicsWorldPhysX::StepSimulation(CHEngine::Timestep deltaTime)
    {
        if (!m_Scene || deltaTime <= 0.0f) return;
        const physx::PxReal stepSeconds = static_cast<physx::PxReal>(deltaTime.GetSeconds());
        m_Scene->simulate(stepSeconds);
        m_Scene->fetchResults(true);
    }

    CHEngine::IPhysicsBody* PhysicsWorldPhysX::CreateRigidBody(const CHEngine::PhysicsRigidBodyDesc& bodyDesc,
                                                                const CHEngine::IPhysicsShape* shape)
    {
        if (!m_Physics || !m_Scene || !shape) return nullptr;

        const auto* shapePhysX = dynamic_cast<const ShapePhysX*>(shape);
        if (!shapePhysX)
        {
            CHE_CORE_ERROR("PhysicsWorldPhysX: shape is not ShapePhysX");
            return nullptr;
        }

        physx::PxGeometryHolder geometryHolder;
        switch (shapePhysX->GetType())
        {
            case PhysXShapeType::Box:
                geometryHolder.storeAny(physx::PxBoxGeometry(ToPxVec3(shapePhysX->GetHalfExtents())));
                break;
            case PhysXShapeType::Sphere:
                geometryHolder.storeAny(physx::PxSphereGeometry(shapePhysX->GetRadius()));
                break;
            case PhysXShapeType::Capsule:
                geometryHolder.storeAny(physx::PxCapsuleGeometry(shapePhysX->GetRadius(), shapePhysX->GetHalfHeight()));
                break;
            default:
                return nullptr;
        }

        physx::PxMaterial* material = m_Physics->createMaterial(
            bodyDesc.StaticFriction, bodyDesc.DynamicFriction, bodyDesc.Restitution);
        if (!material) return nullptr;

        physx::PxRigidActor* actor = nullptr;
        const physx::PxTransform pxTransform = ToPxTransform(bodyDesc.Transform);

        if (bodyDesc.Type == CHEngine::PhysicsBodyType::Static)
        {
            auto* staticActor = m_Physics->createRigidStatic(pxTransform);
            if (!staticActor)
            {
                material->release();
                return nullptr;
            }
            actor = staticActor;
        }
        else
        {
            auto* dynamicActor = m_Physics->createRigidDynamic(pxTransform);
            if (!dynamicActor)
            {
                material->release();
                return nullptr;
            }
            dynamicActor->setLinearDamping(bodyDesc.LinearDamping);
            dynamicActor->setAngularDamping(bodyDesc.AngularDamping);
            dynamicActor->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, !bodyDesc.EnableGravity);

            if (bodyDesc.Type == CHEngine::PhysicsBodyType::Kinematic)
                dynamicActor->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, true);

            actor = dynamicActor;
        }

        physx::PxShape* pxShape = m_Physics->createShape(geometryHolder.any(), *material, true);
        material->release();
        if (!pxShape)
        {
            actor->release();
            return nullptr;
        }

        actor->attachShape(*pxShape);
        pxShape->release();

        if (auto* dynamicActor = actor->is<physx::PxRigidDynamic>())
            physx::PxRigidBodyExt::setMassAndUpdateInertia(*dynamicActor, std::max(0.001f, bodyDesc.Mass));

        m_Scene->addActor(*actor);

        auto body = std::make_unique<BodyPhysX>(actor, bodyDesc.Type);
        BodyPhysX* bodyPtr = body.get();
        actor->userData = bodyPtr;
        m_Bodies.push_back(std::move(body));
        return bodyPtr;
    }

    void PhysicsWorldPhysX::DestroyRigidBody(CHEngine::IPhysicsBody* body)
    {
        if (!body) return;

        auto it = std::find_if(m_Bodies.begin(), m_Bodies.end(),
            [body](const Scope<BodyPhysX>& b) { return b.get() == body; });
        if (it == m_Bodies.end()) return;

        if (m_Scene && (*it)->GetActor())
            m_Scene->removeActor(*(*it)->GetActor(), false);

        m_Bodies.erase(it);
    }

    bool PhysicsWorldPhysX::Raycast(const glm::vec3& origin, const glm::vec3& direction,
                                    float maxDistance, CHEngine::PhysicsRaycastHit& outHit)
    {
        outHit = {};
        if (!m_Scene || maxDistance <= 0.0f) return false;

        physx::PxVec3 dir = ToPxVec3(direction);
        const float magSq = dir.magnitudeSquared();
        if (magSq <= 0.000001f) return false;
        dir.normalize();

        physx::PxRaycastBuffer hitBuffer;
        if (!m_Scene->raycast(ToPxVec3(origin), dir, maxDistance, hitBuffer))
            return false;

        outHit.HasHit = true;
        outHit.Distance = hitBuffer.block.distance;
        outHit.Position = FromPxVec3(hitBuffer.block.position);
        outHit.Normal = FromPxVec3(hitBuffer.block.normal);
        outHit.Body = FindBodyByActor(hitBuffer.block.actor);
        return true;
    }

    bool PhysicsWorldPhysX::OverlapSphere(const glm::vec3& center, float radius,
                                          CHEngine::PhysicsOverlapResult& outResult)
    {
        outResult = {};
        if (!m_Scene || radius <= 0.0f) return false;

        constexpr physx::PxU32 MaxOverlapHits = 64;
        physx::PxOverlapBufferN<MaxOverlapHits> overlapBuffer;

        const physx::PxSphereGeometry geometry(radius);
        const bool hasAnyHit = m_Scene->overlap(geometry, ToPxTransform(center, glm::quat(1.0f, 0.0f, 0.0f, 0.0f)),
                                                overlapBuffer);
        if (!hasAnyHit) return false;

        auto appendOverlapHit = [this, &outResult](const physx::PxOverlapHit& pxHit) {
            CHEngine::PhysicsOverlapHit hit{};
            hit.Body = FindBodyByActor(pxHit.actor);
            outResult.Hits.push_back(hit);
        };
        if (overlapBuffer.hasBlock)
            appendOverlapHit(overlapBuffer.block);
        for (physx::PxU32 i = 0; i < overlapBuffer.nbTouches; ++i)
            appendOverlapHit(overlapBuffer.touches[i]);

        outResult.HasHit = !outResult.Hits.empty();
        return outResult.HasHit;
    }

    bool PhysicsWorldPhysX::OverlapBox(const glm::vec3& center, const glm::vec3& halfExtents,
                                       const glm::quat& rotation, CHEngine::PhysicsOverlapResult& outResult)
    {
        outResult = {};
        if (!m_Scene) return false;
        if (halfExtents.x <= 0.0f || halfExtents.y <= 0.0f || halfExtents.z <= 0.0f) return false;

        constexpr physx::PxU32 MaxOverlapHits = 64;
        physx::PxOverlapBufferN<MaxOverlapHits> overlapBuffer;

        const physx::PxBoxGeometry geometry(ToPxVec3(halfExtents));
        const bool hasAnyHit = m_Scene->overlap(geometry, ToPxTransform(center, rotation), overlapBuffer);
        if (!hasAnyHit) return false;

        auto appendOverlapHit = [this, &outResult](const physx::PxOverlapHit& pxHit) {
            CHEngine::PhysicsOverlapHit hit{};
            hit.Body = FindBodyByActor(pxHit.actor);
            outResult.Hits.push_back(hit);
        };
        if (overlapBuffer.hasBlock)
            appendOverlapHit(overlapBuffer.block);
        for (physx::PxU32 i = 0; i < overlapBuffer.nbTouches; ++i)
            appendOverlapHit(overlapBuffer.touches[i]);

        outResult.HasHit = !outResult.Hits.empty();
        return outResult.HasHit;
    }

    bool PhysicsWorldPhysX::OverlapCapsule(const glm::vec3& center, float radius, float halfHeight,
                                           const glm::quat& rotation, CHEngine::PhysicsOverlapResult& outResult)
    {
        outResult = {};
        if (!m_Scene || radius <= 0.0f || halfHeight <= 0.0f) return false;

        constexpr physx::PxU32 MaxOverlapHits = 64;
        physx::PxOverlapBufferN<MaxOverlapHits> overlapBuffer;

        const physx::PxCapsuleGeometry geometry(radius, halfHeight);
        const bool hasAnyHit = m_Scene->overlap(geometry, ToPxTransform(center, rotation), overlapBuffer);
        if (!hasAnyHit) return false;

        auto appendOverlapHit = [this, &outResult](const physx::PxOverlapHit& pxHit) {
            CHEngine::PhysicsOverlapHit hit{};
            hit.Body = FindBodyByActor(pxHit.actor);
            outResult.Hits.push_back(hit);
        };
        if (overlapBuffer.hasBlock)
            appendOverlapHit(overlapBuffer.block);
        for (physx::PxU32 i = 0; i < overlapBuffer.nbTouches; ++i)
            appendOverlapHit(overlapBuffer.touches[i]);

        outResult.HasHit = !outResult.Hits.empty();
        return outResult.HasHit;
    }

    void PhysicsWorldPhysX::SetContactListener(CHEngine::IPhysicsContactListener* listener)
    {
        m_ContactListener = listener;
    }

    bool PhysicsWorldPhysX::IsValid() const
    {
        return m_Foundation && m_Physics && m_Dispatcher && m_Scene;
    }

    BodyPhysX* PhysicsWorldPhysX::FindBodyByActor(const physx::PxRigidActor* actor) const
    {
        if (!actor || !actor->userData) return nullptr;
        return static_cast<BodyPhysX*>(actor->userData);
    }
}
