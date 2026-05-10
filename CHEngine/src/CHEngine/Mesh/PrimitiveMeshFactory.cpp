#include "chepch.h"
#include "PrimitiveMeshFactory.h"

#include <array>
#include <vector>

namespace CHEngine {

Mesh PrimitiveMeshFactory::CreateCube(float size, const glm::vec3& color)
{
    const float half_size = size * 0.5f;

    const std::array<glm::vec3, 8> corners = {
        glm::vec3{ -half_size, -half_size, -half_size },
        glm::vec3{ half_size, -half_size, -half_size },
        glm::vec3{ half_size, half_size, -half_size },
        glm::vec3{ -half_size, half_size, -half_size },
        glm::vec3{ -half_size, -half_size, half_size },
        glm::vec3{ half_size, -half_size, half_size },
        glm::vec3{ half_size, half_size, half_size },
        glm::vec3{ -half_size, half_size, half_size }
    };

    struct FaceData
    {
        uint32_t corner_indices[4];
        glm::vec3 normal;
    };

    const std::array<FaceData, 6> faces = {
        FaceData{ { 4, 5, 6, 7 }, { 0.0f, 0.0f, 1.0f } },   // +Z
        FaceData{ { 1, 0, 3, 2 }, { 0.0f, 0.0f, -1.0f } },  // -Z
        FaceData{ { 0, 4, 7, 3 }, { -1.0f, 0.0f, 0.0f } },  // -X
        FaceData{ { 5, 1, 2, 6 }, { 1.0f, 0.0f, 0.0f } },   // +X
        FaceData{ { 3, 7, 6, 2 }, { 0.0f, 1.0f, 0.0f } },   // +Y
        FaceData{ { 0, 1, 5, 4 }, { 0.0f, -1.0f, 0.0f } }   // -Y
    };

    const std::array<glm::vec2, 4> uv = {
        glm::vec2{ 0.0f, 0.0f },
        glm::vec2{ 1.0f, 0.0f },
        glm::vec2{ 1.0f, 1.0f },
        glm::vec2{ 0.0f, 1.0f }
    };

    std::vector<Vertex> vertices;
    vertices.reserve(24);
    std::vector<uint32_t> indices;
    indices.reserve(36);

    for (const FaceData& face : faces)
    {
        const uint32_t base_index = static_cast<uint32_t>(vertices.size());
        for (uint32_t i = 0; i < 4; ++i)
        {
            Vertex vertex{};
            vertex.Position = corners[face.corner_indices[i]];
            vertex.Normal = face.normal;
            vertex.TexCoords = uv[i];
            vertex.Color = color;
            vertices.push_back(vertex);
        }

        indices.push_back(base_index + 0);
        indices.push_back(base_index + 1);
        indices.push_back(base_index + 2);
        indices.push_back(base_index + 0);
        indices.push_back(base_index + 2);
        indices.push_back(base_index + 3);
    }

    Mesh mesh;
    mesh.Build(vertices, indices);
    return mesh;
}

} // namespace CHEngine
