#pragma once

#include <Core.h>
#include <glm/glm.hpp>
#include <cstdint>

#include "CHEngine/Mesh/Vertex.h"
#include "Render/Core/RenderTypes.h"
#include "Render/Pipeline/VertexLayout.h"
#include "Material.h"

namespace CHEngine {

	// Standard mesh vertex input layout (pos3 + normal3 + uv2 + color3 = 11 floats).
	const VertexInputLayout& GetStandardMeshLayout();

	struct MeshGpuRecordTag {};
	using MeshGpuRecordHandle = Handle<MeshGpuRecordTag>;

	// Lightweight mesh object stored in MeshLoader's pool.
	// Holds cached buffer handles, index count, and a material instance.
	// Never copy/move directly — use MeshRef or MeshHandle.
	class CHENGINE_API Mesh
	{
	public:
		Mesh() = default;
		Mesh(MeshGpuRecordHandle rec,
		     BufferHandle vb, BufferHandle ib, uint32_t indexCount,
		     Ref<MaterialInstance> mat)
			: m_RecordHandle(rec)
			, m_VertexHandle(vb)
			, m_IndexHandle(ib)
			, m_IndexCount(indexCount)
			, m_Mat(std::move(mat))
		{}

		BufferHandle          GetVertexBuffer()  const { return m_VertexHandle; }
		BufferHandle          GetIndexBuffer()   const { return m_IndexHandle; }
		IndexFormat           GetIndexFormat()   const { return IndexFormat::UInt32; }
		uint32_t              GetIndexCount()    const { return m_IndexCount; }
		bool                  IsValid()          const { return m_IndexCount > 0; }
		Ref<MaterialInstance> GetMatInstance()   const { return m_Mat; }
		MeshGpuRecordHandle   GetRecordHandle()  const { return m_RecordHandle; }

	private:
		friend class MeshLoader;
		MeshGpuRecordHandle   m_RecordHandle;
		BufferHandle          m_VertexHandle;
		BufferHandle          m_IndexHandle;
		uint32_t              m_IndexCount = 0;
		Ref<MaterialInstance> m_Mat;
	};

}
