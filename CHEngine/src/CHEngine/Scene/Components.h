#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "CHEngine/Mesh/Mesh.h"
#include "CHEngine/Scene/SceneObject.h"  // for Transform

namespace CHEngine {

    using TagComponentIDType = uint32_t;

    struct TagComponent {
        std::string         Name;
        TagComponentIDType  ID = 0;
    };

    struct TransformComponent {
        Transform ObjectTransform;
    };

    struct MeshComponent {
        std::vector<Mesh> Meshes;
        std::string       SourcePath;  // original file path for serialization
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
}
