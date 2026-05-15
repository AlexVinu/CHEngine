#include "chepch.h"
#include "Mesh.h"

#include <span>

namespace CHEngine {

	const VertexInputLayout& GetStandardMeshLayout()
	{
		// pos3 + normal3 + uv2 + color3 = 11 floats per vertex (44 bytes stride).
		static const VertexInputLayout kLayout = []() {
			Vector<VertexAttributeDesc> attrs = {
				VertexAttributeDesc{ VertexFormat::Float3, /*slot*/0, /*offset*/0,                         /*sem*/0 },
				VertexAttributeDesc{ VertexFormat::Float3, /*slot*/0, /*offset*/3 * sizeof(float),         /*sem*/0 },
				VertexAttributeDesc{ VertexFormat::Float2, /*slot*/0, /*offset*/6 * sizeof(float),         /*sem*/0 },
				VertexAttributeDesc{ VertexFormat::Float3, /*slot*/0, /*offset*/8 * sizeof(float),         /*sem*/0 },
			};
			return VertexInputLayout(attrs, /*stride*/11 * sizeof(float));
		}();
		return kLayout;
	}

	Mesh::~Mesh()
	{
		if (m_Handle.IsValid())
			MeshLoader::Instance().Release(m_Handle);
	}

	Mesh::Mesh(const Mesh& other)
		: Mat(other.Mat)
		, m_Handle(other.m_Handle)
	{
		if (m_Handle.IsValid())
			MeshLoader::Instance().AddRef(m_Handle);
	}

	Mesh& Mesh::operator=(const Mesh& other)
	{
		if (this == &other) return *this;

		if (m_Handle.IsValid())
			MeshLoader::Instance().Release(m_Handle);

		Mat = other.Mat;
		m_Handle = other.m_Handle;
		if (m_Handle.IsValid())
			MeshLoader::Instance().AddRef(m_Handle);
		return *this;
	}

	Mesh::Mesh(Mesh&& other) noexcept
		: Mat(std::move(other.Mat))
		, m_Handle(other.m_Handle)
	{
		other.m_Handle = {};
	}

	Mesh& Mesh::operator=(Mesh&& other) noexcept
	{
		if (this != &other)
		{
			if (m_Handle.IsValid())
				MeshLoader::Instance().Release(m_Handle);

			Mat = std::move(other.Mat);
			m_Handle = other.m_Handle;
			other.m_Handle = {};
		}
		return *this;
	}

	void Mesh::Build(const std::vector<Vertex>& vertices,
	                 const std::vector<uint32_t>& indices)
	{
		if (m_Handle.IsValid())
		{
			MeshLoader::Instance().Release(m_Handle);
			m_Handle = {};
		}
		m_Handle = MeshLoader::Instance().GetOrCreate(vertices, indices);
	}

	BufferHandle Mesh::GetVertexBuffer() const
	{
		const MeshGpuRecord* r = MeshLoader::Instance().Get(m_Handle);
		return r ? r->vb : BufferHandle{};
	}

	BufferHandle Mesh::GetIndexBuffer() const
	{
		const MeshGpuRecord* r = MeshLoader::Instance().Get(m_Handle);
		return r ? r->ib : BufferHandle{};
	}

	uint32_t Mesh::GetIndexCount() const
	{
		const MeshGpuRecord* r = MeshLoader::Instance().Get(m_Handle);
		return r ? r->indexCount : 0u;
	}

	const std::vector<Vertex>& Mesh::GetVertices() const
	{
		static const std::vector<Vertex> kEmpty;
		const MeshGpuRecord* r = MeshLoader::Instance().Get(m_Handle);
		return r ? r->vertices : kEmpty;
	}

	const std::vector<uint32_t>& Mesh::GetIndices() const
	{
		static const std::vector<uint32_t> kEmpty;
		const MeshGpuRecord* r = MeshLoader::Instance().Get(m_Handle);
		return r ? r->indices : kEmpty;
	}

	void Mesh::UpdateUVs(std::span<const glm::vec2> uvs)
	{
		if (!m_Handle.IsValid()) return;
		MeshLoader::Instance().UpdateVertexUVs(m_Handle, uvs);
	}

}
