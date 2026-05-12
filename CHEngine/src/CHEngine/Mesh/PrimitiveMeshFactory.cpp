#include "chepch.h"
#include "PrimitiveMeshFactory.h"

#include <array>
#include <vector>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

Mesh PrimitiveMeshFactory::CreateSphere(float radius, uint32_t segments, uint32_t slices, const glm::vec3& color)
{
    segments = std::max(segments, 3u);
    slices   = std::max(slices, 3u);

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    const uint32_t vertCount = (slices - 1) * segments + 2;
    const uint32_t idxCount  = (slices - 2) * segments * 6 + segments * 6;
    vertices.reserve(vertCount);
    indices.reserve(idxCount);

    // Top pole
    vertices.push_back({ {0.0f, radius, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.5f, 0.0f}, color });

    for (uint32_t j = 1; j < slices; ++j)
    {
            float theta = static_cast<float>(M_PI) * static_cast<float>(j) / static_cast<float>(slices);

        for (uint32_t i = 0; i < segments; ++i)
        {
            float phi = 2.0f * static_cast<float>(M_PI) * static_cast<float>(i) / static_cast<float>(segments);

            glm::vec3 pos{
                radius * glm::sin(theta) * glm::cos(phi),
                radius * glm::cos(theta),
                radius * glm::sin(theta) * glm::sin(phi)
            };
            glm::vec3 normal = glm::normalize(pos);

            float u = static_cast<float>(i) / static_cast<float>(segments);
            float v = static_cast<float>(j) / static_cast<float>(slices);

            vertices.push_back({ pos, normal, {u, v}, color });
        }
    }

    // Bottom pole
    vertices.push_back({ {0.0f, -radius, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.5f, 1.0f}, color });

    const uint32_t poleTop = 0;
    const uint32_t poleBot = 1 + (slices - 1) * segments;

    // Top ring (triangles from top pole to first ring)
    for (uint32_t i = 0; i < segments; ++i)
    {
        uint32_t a = 1 + i;
        uint32_t b = 1 + ((i + 1) % segments);
        indices.push_back(poleTop);
        indices.push_back(a);
        indices.push_back(b);
    }

    // Middle rings (quad strips)
    for (uint32_t j = 0; j < slices - 2; ++j)
    {
        for (uint32_t i = 0; i < segments; ++i)
        {
            uint32_t a = 1 + j * segments + i;
            uint32_t b = 1 + j * segments + ((i + 1) % segments);
            uint32_t c = 1 + (j + 1) * segments + ((i + 1) % segments);
            uint32_t d = 1 + (j + 1) * segments + i;

            indices.push_back(a);
            indices.push_back(b);
            indices.push_back(c);
            indices.push_back(a);
            indices.push_back(c);
            indices.push_back(d);
        }
    }

    // Bottom ring (triangles from bottom pole to last ring)
    for (uint32_t i = 0; i < segments; ++i)
    {
        uint32_t a = poleBot - segments + i;
        uint32_t b = poleBot - segments + ((i + 1) % segments);
        indices.push_back(poleBot);
        indices.push_back(a);
        indices.push_back(b);
    }

    Mesh mesh;
    mesh.Build(vertices, indices);
    return mesh;
}

} // namespace CHEngine
