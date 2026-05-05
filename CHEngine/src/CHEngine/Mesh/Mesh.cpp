#include "chepch.h"
#include "Mesh.h"

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
		// Note: buffers are owned by the factory and freed via RenderFacade::GetRenderFactory().
		// Phase 6 will introduce explicit destroy via factory->Delete(BufferHandle).
		// For now we leave them — leaking until shutdown is acceptable in this transitional phase.
	}

	Mesh::Mesh(Mesh&& other) noexcept
		: Mat(std::move(other.Mat))
		, m_VertexBuffer(other.m_VertexBuffer)
		, m_IndexBuffer(other.m_IndexBuffer)
		, m_IndexCount(other.m_IndexCount)
		, m_Vertices(std::move(other.m_Vertices))
		, m_Indices(std::move(other.m_Indices))
	{
		other.m_VertexBuffer = {};
		other.m_IndexBuffer  = {};
		other.m_IndexCount   = 0;
	}

	Mesh& Mesh::operator=(Mesh&& other) noexcept
	{
		if (this != &other)
		{
			Mat = std::move(other.Mat);
			m_VertexBuffer = other.m_VertexBuffer;
			m_IndexBuffer  = other.m_IndexBuffer;
			m_IndexCount = other.m_IndexCount;
			m_Vertices = std::move(other.m_Vertices);
			m_Indices = std::move(other.m_Indices);
			other.m_VertexBuffer = {};
			other.m_IndexBuffer  = {};
			other.m_IndexCount = 0;
		}
		return *this;
	}

	void Mesh::Build(const std::vector<Vertex>& vertices,
	                 const std::vector<uint32_t>& indices)
	{
		m_Vertices = vertices;
		m_Indices  = indices;
		m_IndexCount = static_cast<uint32_t>(indices.size());

		IRenderFactory* f = RenderFacade::GetRenderFactory();
		if (!f) { CHE_CORE_ERROR("Mesh::Build — no render factory"); return; }

		// Flatten vertex data: pos(3) + normal(3) + uv(2) + color(3) = 11 floats.
		std::vector<float> flatData;
		flatData.reserve(vertices.size() * 11);
		for (const auto& v : vertices)
		{
			flatData.push_back(v.Position.x);
			flatData.push_back(v.Position.y);
			flatData.push_back(v.Position.z);
			flatData.push_back(v.Normal.x);
			flatData.push_back(v.Normal.y);
			flatData.push_back(v.Normal.z);
			flatData.push_back(v.TexCoords.x);
			flatData.push_back(v.TexCoords.y);
			flatData.push_back(v.Color.x);
			flatData.push_back(v.Color.y);
			flatData.push_back(v.Color.z);
		}

		const uint64_t vbSize = flatData.size() * sizeof(float);
		std::span<const std::byte> vbBytes{
			reinterpret_cast<const std::byte*>(flatData.data()), vbSize };
		m_VertexBuffer = f->CreateBuffer(vbSize, BufferUsage::Vertex,
		                                 MemoryType::GpuOnly, vbBytes,
		                                 String("mesh_vb"));

		const uint64_t ibSize = indices.size() * sizeof(uint32_t);
		std::span<const std::byte> ibBytes{
			reinterpret_cast<const std::byte*>(indices.data()), ibSize };
		m_IndexBuffer = f->CreateBuffer(ibSize, BufferUsage::Index,
		                                MemoryType::GpuOnly, ibBytes,
		                                String("mesh_ib"));
	}

}
