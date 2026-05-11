#pragma once

#include <Core.h>
#include <glm/glm.hpp>
#include <memory>
#include <span>
#include <vector>
#include <cstdint>

#include "CHEngine/Mesh/Vertex.h"
#include "CHEngine/ResourceManager/MeshLoader.h"
#include "Render/Core/RenderTypes.h"
#include "Render/Pipeline/VertexLayout.h"
#include "Material.h"

namespace CHEngine {

	// Standard mesh vertex input layout (pos3 + normal3 + uv2 + color3 = 11 floats).
	const VertexInputLayout& GetStandardMeshLayout();

	class CHENGINE_API Mesh
	{
	public:
		Mesh() = default;
		~Mesh();

		Mesh(const Mesh& other);
		Mesh& operator=(const Mesh& other);

		Mesh(Mesh&& other) noexcept;
		Mesh& operator=(Mesh&& other) noexcept;

		void Build(const std::vector<Vertex>& vertices,
		           const std::vector<uint32_t>& indices);

		BufferHandle GetVertexBuffer() const;
		BufferHandle GetIndexBuffer()  const;
		IndexFormat  GetIndexFormat()  const { return IndexFormat::UInt32; }
		uint32_t     GetIndexCount()   const;
		bool         IsValid()         const { return GetIndexCount() > 0; }

		const std::vector<Vertex>&   GetVertices() const;
		const std::vector<uint32_t>& GetIndices()  const;

		// Update UV coordinates on CPU + GPU at runtime.
		void UpdateUVs(std::span<const glm::vec2> uvs);

		MeshHandle GetCacheHandle() const { return m_Handle; }

		Ref<MaterialInstance> Mat;

	private:
		MeshHandle m_Handle{};
	};

}
