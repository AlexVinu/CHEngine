#pragma once

#include <Core.h>
#include <glm/glm.hpp>
#include <cstdint>
#include <vector>

#include "CHEngine/Mesh/Vertex.h"
#include "Render/Core/RenderTypes.h"
#include "Render/Pipeline/VertexLayout.h"
#include "Material.h"

namespace CHEngine {

	// Standard mesh vertex input layout (pos3 + normal3 + uv2 + color3 = 11 floats).
	const VertexInputLayout& GetStandardMeshLayout();

	struct MeshGpuRecordTag {};
	using MeshGpuRecordHandle = Handle<MeshGpuRecordTag>;

	struct SubMesh {
		uint32_t indexCount  = 0;
		uint32_t startIndex  = 0;
		int32_t  baseVertex  = 0;
	};

	// Logical mesh: shares geometry via MeshGpuRecord, owns its own per-submesh materials.
	// Never copy/move directly — use MeshRef or MeshHandle.
	class CHENGINE_API Mesh
	{
	public:
		Mesh() = default;
		Mesh(MeshGpuRecordHandle rec, std::vector<Ref<MaterialInstance>> materials)
			: m_RecordHandle(rec)
			, m_Materials(std::move(materials))
		{}

		MeshGpuRecordHandle   GetRecordHandle()  const { return m_RecordHandle; }
		IndexFormat           GetIndexFormat()   const { return IndexFormat::UInt32; }

		const std::vector<Ref<MaterialInstance>>& GetMaterials() const { return m_Materials; }
		Ref<MaterialInstance> GetMaterial(uint32_t submeshIndex) const {
			return submeshIndex < m_Materials.size() ? m_Materials[submeshIndex] : nullptr;
		}
		uint32_t GetSubMeshCount() const { return static_cast<uint32_t>(m_Materials.size()); }

		// Buffers / submeshes are accessed via MeshLoader::GetGpuRecord(handle).
		bool IsValid() const { return !m_Materials.empty(); }

	private:
		friend class MeshLoader;
		MeshGpuRecordHandle                  m_RecordHandle;
		std::vector<Ref<MaterialInstance>>   m_Materials;
	};

}
