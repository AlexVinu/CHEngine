#pragma once

#include <Core.h>
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

#include "CHEngine/Render/RenderResourceManager.h"
#include "CHEngine/Mesh/Material.h"

namespace CHEngine {

	struct Vertex {
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec2 TexCoords;
		glm::vec3 Color;
	};

	class CHENGINE_API Mesh
	{
	public:
		Mesh() = default;

		void Build(RenderResourceManager& resources,
		           const std::vector<Vertex>& vertices,
		           const std::vector<uint32_t>& indices);

		VertexArrayHandle GetVertexArray() const { return m_VertexArray; }
		uint32_t GetIndexCount() const { return m_IndexCount; }
		bool IsValid() const { return m_IndexCount > 0; }

		const std::vector<Vertex>& GetVertices() const { return m_Vertices; }
		const std::vector<uint32_t>& GetIndices() const { return m_Indices; }

		// Материал меша — диффузная/specular текстуры и параметры освещения.
		Material Mat;

	private:
		VertexArrayHandle m_VertexArray;
		uint32_t m_IndexCount = 0;

		std::vector<Vertex> m_Vertices;
		std::vector<uint32_t> m_Indices;
	};

}
