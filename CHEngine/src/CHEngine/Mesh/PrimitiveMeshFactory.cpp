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

    // Cross UV layout: 1 top, 3 middle, 1 below, 1 bottom — like Blender
    //   Face order: 0=+Z, 1=-Z, 2=-X, 3=+X, 4=+Y, 5=-Y
    const int faceCol[6] = { 1, 1, 0, 2, 1, 1 };
    const int faceRow[6] = { 1, 2, 1, 1, 0, 3 };
    constexpr float cell = 0.25f;

    std::vector<Vertex> vertices;
    vertices.reserve(24);
    std::vector<uint32_t> indices;
    indices.reserve(36);

    for (int f = 0; f < 6; ++f)
    {
        const FaceData& face = faces[f];
        const uint32_t base_index = static_cast<uint32_t>(vertices.size());

        const float u_min = static_cast<float>(faceCol[f]) * cell;
        const float u_max = u_min + cell;
        const float v_min = static_cast<float>(faceRow[f]) * cell;
        const float v_max = v_min + cell;

        const std::array<glm::vec2, 4> face_uvs = {{
            { u_min, v_min },
            { u_max, v_min },
            { u_max, v_max },
            { u_min, v_max }
        }};

        for (uint32_t i = 0; i < 4; ++i)
        {
            Vertex vertex{};
            vertex.Position = corners[face.corner_indices[i]];
            vertex.Normal = face.normal;
            vertex.TexCoords = face_uvs[i];
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
