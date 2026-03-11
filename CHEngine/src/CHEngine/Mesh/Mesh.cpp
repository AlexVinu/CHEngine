#include "chepch.h"
#include "Mesh.h"

namespace CHEngine {

	void Mesh::Build(RenderResourceManager& resources,
	                 const std::vector<Vertex>& vertices,
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

		m_VertexArray = resources.CreateVertexArray();

		auto vb = resources.CreateVertexBuffer(flatData.data(),
			static_cast<uint32_t>(flatData.size() * sizeof(float)));

		BufferLayout layout = {
			{ ShaderDataType::Float3, "a_Position" },
			{ ShaderDataType::Float3, "a_Normal" },
			{ ShaderDataType::Float2, "a_TexCoords" },
			{ ShaderDataType::Float3, "a_Color" },
		};
		vb->SetLayout(layout);
		resources.Get(m_VertexArray)->AddVertexBuffer(vb);

		std::vector<uint32_t> idxCopy = indices;
		auto ib = resources.CreateIndexBuffer(idxCopy.data(),
			static_cast<uint32_t>(idxCopy.size()));
		resources.Get(m_VertexArray)->SetIndexBuffer(ib);
	}

}
