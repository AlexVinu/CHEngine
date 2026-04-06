#include "chepch.h"

#include "BodyPhysX.h"

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

    glm::quat FromPxQuat(const physx::PxQuat& q)
    {
        return glm::quat(q.w, q.x, q.y, q.z);
    }

    physx::PxTransform ToPxTransform(const CHEngine::PhysicsTransform& t)
    {
        return { ToPxVec3(t.Position), ToPxQuat(t.Rotation) };
    }

    CHEngine::PhysicsTransform FromPxTransform(const physx::PxTransform& t)
    {
        CHEngine::PhysicsTransform out{};
        out.Position = FromPxVec3(t.p);
        out.Rotation = FromPxQuat(t.q);
        return out;
    }

    physx::PxForceMode::Enum ToPxForceMode(CHEngine::PhysicsForceMode mode)
    {
        switch (mode)
        {
            case CHEngine::PhysicsForceMode::Impulse:        return physx::PxForceMode::eIMPULSE;
            case CHEngine::PhysicsForceMode::VelocityChange: return physx::PxForceMode::eVELOCITY_CHANGE;
            case CHEngine::PhysicsForceMode::Acceleration:   return physx::PxForceMode::eACCELERATION;
            default:                                          return physx::PxForceMode::eFORCE;
        }
    }
}

namespace CHModules
{
    BodyPhysX::BodyPhysX(physx::PxRigidActor* actor, CHEngine::PhysicsBodyType bodyType)
        : m_Actor(actor), m_BodyType(bodyType)
    {
    }

    BodyPhysX::~BodyPhysX()
    {
        if (m_Actor)
        {
            m_Actor->release();
            m_Actor = nullptr;
        }
    }

    CHEngine::PhysicsTransform BodyPhysX::GetTransform() const
    {
        if (!m_Actor) return {};
        return FromPxTransform(m_Actor->getGlobalPose());
    }

    void BodyPhysX::SetTransform(const CHEngine::PhysicsTransform& transform)
    {
        if (!m_Actor) return;
        m_Actor->setGlobalPose(ToPxTransform(transform), true);
    }

    void BodyPhysX::SetKinematicTarget(const CHEngine::PhysicsTransform& transform)
    {
        if (!m_Actor) return;
        auto* dynamicActor = m_Actor->is<physx::PxRigidDynamic>();
        if (!dynamicActor) return;
        dynamicActor->setKinematicTarget(ToPxTransform(transform));
    }

    glm::vec3 BodyPhysX::GetLinearVelocity() const
    {
        if (!m_Actor) return {};
        auto* dynamicActor = m_Actor->is<physx::PxRigidDynamic>();
        if (!dynamicActor) return {};
        return FromPxVec3(dynamicActor->getLinearVelocity());
    }

    void BodyPhysX::SetLinearVelocity(const glm::vec3& velocity)
    {
        if (!m_Actor) return;
        auto* dynamicActor = m_Actor->is<physx::PxRigidDynamic>();
        if (!dynamicActor) return;
        dynamicActor->setLinearVelocity(ToPxVec3(velocity));
    }

    void BodyPhysX::AddForce(const glm::vec3& force, CHEngine::PhysicsForceMode mode)
    {
        if (!m_Actor) return;
        auto* dynamicActor = m_Actor->is<physx::PxRigidDynamic>();
        if (!dynamicActor) return;
        dynamicActor->addForce(ToPxVec3(force), ToPxForceMode(mode));
    }
}
