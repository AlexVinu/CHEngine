#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace CHEngine
{
    class IPhysicsBody;

    struct PhysicsTransform
    {
        glm::vec3 Position{ 0.0f };
        glm::quat Rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
    };

    enum class PhysicsBodyType : uint8_t
    {
        Static    = 0,
        Dynamic   = 1,
        Kinematic = 2
    };

    enum class PhysicsForceMode : uint8_t
    {
        Force          = 0,
        Impulse        = 1,
        VelocityChange = 2,
        Acceleration   = 3
    };

    struct PhysicsWorldDesc
    {
        glm::vec3 Gravity{ 0.0f, -9.81f, 0.0f };
        uint32_t  SolverThreads = 1;
    };

    struct PhysicsRigidBodyDesc
    {
        PhysicsBodyType Type = PhysicsBodyType::Dynamic;
        PhysicsTransform Transform{};

        float Mass            = 1.0f;
        float LinearDamping   = 0.0f;
        float AngularDamping  = 0.05f;
        bool  EnableGravity   = true;
        float StaticFriction  = 0.5f;
        float DynamicFriction = 0.5f;
        float Restitution     = 0.1f;
    };

    struct PhysicsRaycastHit
    {
        bool          HasHit   = false;
        float         Distance = 0.0f;
        glm::vec3     Position{};
        glm::vec3     Normal{};
        IPhysicsBody* Body = nullptr;
    };

    struct PhysicsOverlapHit
    {
        IPhysicsBody* Body = nullptr;
    };

    struct PhysicsOverlapResult
    {
        bool HasHit = false;
        std::vector<PhysicsOverlapHit> Hits{};
    };

    struct PhysicsContactEvent
    {
        IPhysicsBody* BodyA = nullptr;
        IPhysicsBody* BodyB = nullptr;
        glm::vec3     Point{};
        glm::vec3     Normal{};
    };
}
