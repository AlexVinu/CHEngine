#pragma once
#include <string>
#include <cstdint>
#include <vector>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/nil_generator.hpp>
#include "CHEngine/Mesh/Mesh.h"
#include "Light.h"
#include "Transform.h"
#include <Physics/PhysicsTypes.h>
#include <Physics/IPhysicsBody.h>
#include <Physics/IPhysicsShape.h>

// Only components

namespace CHEngine {

    using UUID = boost::uuids::uuid;
    class IPhysicsBody;
    class IPhysicsShape;

    struct IDComponent
    {
        UUID Value = boost::uuids::nil_uuid();
        IDComponent() = default;
        explicit IDComponent(const UUID& uuid)
            : Value(uuid)
        {
        }
    };

    struct TagComponent {
        std::string         Name;
    };

    // Render
    struct TransformComponent {
        Transform ObjectTransform;
    };

    struct MeshComponent {
        std::vector<Mesh> Meshes;
        std::string       SourcePath;  // original file path for serialization

        MeshComponent() = default;
        MeshComponent(const MeshComponent&) = delete;
        MeshComponent& operator=(const MeshComponent&) = delete;
        MeshComponent(MeshComponent&&) = default;
        MeshComponent& operator=(MeshComponent&&) = default;
    };

    struct ColorComponent {
        glm::vec4 Color = { 0.8f, 0.8f, 0.8f, 1.0f };
    };

    struct VisibilityComponent {
        bool Visible = true;
    };

    struct LightComponent {
        Light LightData;
    };

    struct CameraComponent
    {
        Transform CameraTransform;
        float FOV = 45.0f;
        float NearClip = 0.1f;
        float FarClip = 1000.0f;
        float AspectRatio = 16.0f / 9.0f;
        bool Active = true;
        bool Primary = true;
    };

    // Physics

    struct RigidBody3DComponent
    {
        PhysicsRigidBodyDesc BodyDesc{};
        PhysicsColliderShapeDesc ShapeDesc{};
        RigidBodySyncMode SyncMode = RigidBodySyncMode::Auto;

        IPhysicsBody* Body = nullptr;
        IPhysicsShape* Shape = nullptr;
    };

    struct TransformDirtyComponent
    {
        bool Dirty = true;
    };

    struct LifetimeComponent
    {
        float RemainingSeconds = 0.0f;
        bool DestroyOnExpire = true;
    };
}
