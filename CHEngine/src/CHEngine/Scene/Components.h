#pragma once
#include <string>
#include <cstdint>
#include <vector>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/nil_generator.hpp>
#include "CHEngine/Mesh/Mesh.h"
#include "Light.h"
#include "Transform.h"
#include "Physics/IPhysicsBody.h"

namespace CHEngine {

    using UUID = boost::uuids::uuid;

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

    // Physics
    enum class RigidBodySyncMode : uint8_t
    {
        Auto = 0,
        ReadFromPhysics = 1,
        WriteToPhysics = 2,
        ReadWrite = 3
    };

    struct RigidBody3DComponent
    {
        IPhysicsBody* Body = nullptr;
        RigidBodySyncMode SyncMode = RigidBodySyncMode::Auto;
    };
}
