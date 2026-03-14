#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "CHEngine/Mesh/Mesh.h"
#include "CHEngine/Scene/SceneObject.h"  // for Transform

namespace CHEngine {

    struct TagComponent {
        std::string Name;
        uint32_t    ID = 0;
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
}
