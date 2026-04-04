#include "chepch.h"
#include "Mesh.h"

namespace CHEngine {

	Mesh::~Mesh()
	{
		if (m_VertexArray.IsValid())
			RenderFacade::DestroyVertexArray(m_VertexArray);
	}

	Mesh::Mesh(Mesh&& other) noexcept
		: Mat(std::move(other.Mat))
		, m_VertexArray(other.m_VertexArray)
		, m_IndexCount(other.m_IndexCount)
		, m_Vertices(std::move(other.m_Vertices))
		, m_Indices(std::move(other.m_Indices))
	{
		other.m_VertexArray = {};
		other.m_IndexCount = 0;
	}

	Mesh& Mesh::operator=(Mesh&& other) noexcept
	{
		if (this != &other)
		{
			if (m_VertexArray.IsValid())
				RenderFacade::DestroyVertexArray(m_VertexArray);
			Mat = std::move(other.Mat);
			m_VertexArray = other.m_VertexArray;
			m_IndexCount = other.m_IndexCount;
			m_Vertices = std::move(other.m_Vertices);
			m_Indices = std::move(other.m_Indices);
			other.m_VertexArray = {};
			other.m_IndexCount = 0;
		}
		return *this;
	}

	void Mesh::Build(const std::vector<Vertex>& vertices,
	                 const std::vector<uint32_t>& indices)
	{
		m_Vertices = vertices;
		m_Indices = indices;
		m_IndexCount = static_cast<uint32_t>(indices.size());

		// Flatten vertex data: pos(3) + normal(3) + uv(2) + color(3) = 11 floats
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

		m_VertexArray = RenderFacade::CreateVertexArray();
		auto* vao = RenderFacade::GetVertexArray(m_VertexArray);
		if (!vao)
		{
			CHE_CORE_ERROR("Mesh::Build — failed to create vertex array");
			return;
		}

		auto vb = RenderFacade::CreateVertexBuffer(flatData.data(),
			static_cast<uint32_t>(flatData.size() * sizeof(float)));

		BufferLayout layout = {
			{ ShaderDataType::Float3, "a_Position" },
			{ ShaderDataType::Float3, "a_Normal" },
			{ ShaderDataType::Float2, "a_TexCoords" },
			{ ShaderDataType::Float3, "a_Color" },
		};
		vb->SetLayout(layout);
		vao->AddVertexBuffer(vb);

		auto ib = RenderFacade::CreateIndexBuffer(m_Indices.data(),
			static_cast<uint32_t>(m_Indices.size()));
		vao->SetIndexBuffer(ib);
	}

}
