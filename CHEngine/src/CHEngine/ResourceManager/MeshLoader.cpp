#include "chepch.h"
#include "MeshLoader.h"

#include "CHEngine/Application.h"
#include "Render/Core/RenderTypes.h"
#include "Log/Log.h"

#include <algorithm>
#include <cstring>
#include <span>

namespace CHEngine
{
	namespace
	{
		constexpr uint64_t SEED = 0xCBF29CE484222325ULL;
		constexpr uint64_t MUL  = 0xc6a4a7935bd1e995ULL;

		inline uint64_t Hash64Bits(uint64_t val, uint64_t seed) noexcept
		{
			return (seed ^= val) * MUL;
		}

		uint64_t HashBytes(const void* data, size_t bytes, uint64_t seed) noexcept
		{
			const uint8_t* ptr = reinterpret_cast<const uint8_t*>(data);
			while (bytes >= 8)
			{
				uint64_t val;
				std::memcpy(&val, ptr, 8);
				seed = Hash64Bits(val, seed);
				ptr   += 8;
				bytes -= 8;
			}
			if (bytes > 0)
			{
				uint64_t val = 0;
				std::memcpy(&val, ptr, bytes);
				seed = Hash64Bits(val, seed);
			}
			return seed;
		}

		uint64_t ComputeContentHash(const std::vector<Vertex>& vertices,
		                            const std::vector<uint32_t>& indices)
		{
			uint64_t h = SEED;
			if (!vertices.empty())
				h = HashBytes(vertices.data(), vertices.size() * sizeof(Vertex), h);
			if (!indices.empty())
				h = HashBytes(indices.data(), indices.size() * sizeof(uint32_t), h);
			h ^= (static_cast<uint64_t>(vertices.size()) << 32) ^ indices.size();
			return h;
		}

		bool ContentEqual(const MeshGpuRecord& r,
		                  const std::vector<Vertex>& vertices,
		                  const std::vector<uint32_t>& indices)
		{
			if (r.vertices.size() != vertices.size() || r.indices.size() != indices.size())
				return false;
			if (!vertices.empty() &&
			    std::memcmp(r.vertices.data(), vertices.data(), vertices.size() * sizeof(Vertex)) != 0)
				return false;
			if (!indices.empty() &&
			    std::memcmp(r.indices.data(), indices.data(), indices.size() * sizeof(uint32_t)) != 0)
				return false;
			return true;
		}
	}

	MeshHandle MeshLoader::GetOrCreate(const std::vector<Vertex>& vertices,
	                                   const std::vector<uint32_t>& indices,
	                                   const std::vector<SubMesh>& subMeshes,
	                                   std::vector<Ref<MaterialInstance>> materials)
	{
		if (vertices.empty() || indices.empty() || subMeshes.empty())
			return {};

		if (materials.size() != subMeshes.size())
		{
			CHE_CORE_ERROR("MeshLoader::GetOrCreate — materials.size()={} != subMeshes.size()={}",
			               materials.size(), subMeshes.size());
			return {};
		}

		const uint64_t hash = ComputeContentHash(vertices, indices);

		// Search existing records for a content match.
		MeshGpuRecordHandle recordHandle;
		auto it = m_HashBuckets.find(hash);
		if (it != m_HashBuckets.end())
		{
			for (MeshGpuRecordHandle rh : it->second)
			{
				MeshGpuRecord* r = m_Records.Get(rh);
				if (r && ContentEqual(*r, vertices, indices))
				{
					CHE_CORE_INFO("MeshLoader: GpuRecord reused (hash={:x})", hash);
					recordHandle = rh;
					break;
				}
			}
		}

		if (!recordHandle.IsValid())
		{
			IRenderFactory* f = Application::Get().Render().GetRenderFactory();
			if (!f) { CHE_CORE_ERROR("MeshLoader::GetOrCreate — no render factory"); return {}; }

			const uint64_t vbSize = vertices.size() * sizeof(Vertex);
			BufferHandle vb = f->CreateBuffer(vbSize, BufferUsage::Vertex, MemoryType::GpuOnly,
			    std::span<const std::byte>{ reinterpret_cast<const std::byte*>(vertices.data()), vbSize },
			    String("mesh_vb"));

			const uint64_t ibSize = indices.size() * sizeof(uint32_t);
			BufferHandle ib = f->CreateBuffer(ibSize, BufferUsage::Index, MemoryType::GpuOnly,
			    std::span<const std::byte>{ reinterpret_cast<const std::byte*>(indices.data()), ibSize },
			    String("mesh_ib"));

			auto* rec        = new MeshGpuRecord{};
			rec->vb          = vb;
			rec->ib          = ib;
			rec->subMeshes   = subMeshes;
			rec->contentHash = hash;
			rec->refcount    = 0;
			rec->vertices    = vertices;
			rec->indices     = indices;

			recordHandle = m_Records.Add(rec);
			m_HashBuckets[hash].push_back(recordHandle);
		}

		MeshGpuRecord* rec = m_Records.Get(recordHandle);
		++rec->refcount;

		auto* mesh = new Mesh(recordHandle, std::move(materials));
		return m_Meshes.Add(mesh);
	}

	MeshHandle MeshLoader::GetOrCreate(const std::vector<Vertex>& vertices,
	                                   const std::vector<uint32_t>& indices,
	                                   Ref<MaterialInstance> mat)
	{
		if (vertices.empty() || indices.empty())
			return {};

		std::vector<SubMesh> subs = {
			SubMesh{ static_cast<uint32_t>(indices.size()), 0u, 0 }
		};
		std::vector<Ref<MaterialInstance>> mats = { std::move(mat) };
		return GetOrCreate(vertices, indices, subs, std::move(mats));
	}

	MeshHandle MeshLoader::AddRef(MeshHandle h)
	{
		const Mesh* src = m_Meshes.Get(h);
		if (!src) return {};

		MeshGpuRecord* rec = m_Records.Get(src->m_RecordHandle);
		if (!rec) return {};

		++rec->refcount;
		// Copy materials so each instance can override independently.
		auto* mesh = new Mesh(src->m_RecordHandle, src->m_Materials);
		return m_Meshes.Add(mesh);
	}

	void MeshLoader::Release(MeshHandle h)
	{
		Mesh* mesh = m_Meshes.Get(h);
		if (!mesh) return;

		const MeshGpuRecordHandle recordHandle = mesh->m_RecordHandle;
		m_Meshes.Remove(h); // deletes the Mesh object

		MeshGpuRecord* rec = m_Records.Get(recordHandle);
		if (!rec) return;

		if (--rec->refcount == 0)
			DestroyRecord(recordHandle, rec);
	}

	Mesh* MeshLoader::Get(MeshHandle h)
	{
		return m_Meshes.Get(h);
	}

	const Mesh* MeshLoader::Get(MeshHandle h) const
	{
		return m_Meshes.Get(h);
	}

	const MeshGpuRecord* MeshLoader::GetGpuRecord(MeshHandle h) const
	{
		const Mesh* mesh = m_Meshes.Get(h);
		if (!mesh) return nullptr;
		return m_Records.Get(mesh->m_RecordHandle);
	}

	void MeshLoader::SetMaterial(MeshHandle h, uint32_t submeshIndex, Ref<MaterialInstance> mat)
	{
		Mesh* mesh = m_Meshes.Get(h);
		if (!mesh) return;
		if (submeshIndex >= mesh->m_Materials.size())
		{
			CHE_CORE_ERROR("MeshLoader::SetMaterial — submeshIndex {} out of range (count={})",
			               submeshIndex, mesh->m_Materials.size());
			return;
		}
		mesh->m_Materials[submeshIndex] = std::move(mat);
	}

	void MeshLoader::UpdateVertexUVs(MeshHandle h, uint32_t submeshIndex, std::span<const glm::vec2> uvs)
	{
		Mesh* mesh = m_Meshes.Get(h);
		if (!mesh) return;

		MeshGpuRecord* rec = m_Records.Get(mesh->m_RecordHandle);
		if (!rec) return;

		if (submeshIndex >= rec->subMeshes.size())
		{
			CHE_CORE_ERROR("MeshLoader::UpdateVertexUVs — submeshIndex {} out of range (count={})",
			               submeshIndex, rec->subMeshes.size());
			return;
		}

		const SubMesh sm = rec->subMeshes[submeshIndex];

		// Collect unique vertex indices used by this submesh.
		std::vector<uint32_t> uniqueVerts;
		uniqueVerts.reserve(sm.indexCount);
		const uint32_t end = sm.startIndex + sm.indexCount;
		if (end > rec->indices.size())
		{
			CHE_CORE_ERROR("MeshLoader::UpdateVertexUVs — submesh index range exceeds index buffer");
			return;
		}
		for (uint32_t i = sm.startIndex; i < end; ++i)
		{
			const int64_t vi = static_cast<int64_t>(rec->indices[i]) + sm.baseVertex;
			if (vi < 0 || static_cast<size_t>(vi) >= rec->vertices.size())
			{
				CHE_CORE_ERROR("MeshLoader::UpdateVertexUVs — vertex index out of range");
				return;
			}
			uniqueVerts.push_back(static_cast<uint32_t>(vi));
		}
		std::sort(uniqueVerts.begin(), uniqueVerts.end());
		uniqueVerts.erase(std::unique(uniqueVerts.begin(), uniqueVerts.end()), uniqueVerts.end());

		if (uvs.size() != uniqueVerts.size())
		{
			CHE_CORE_ERROR("MeshLoader::UpdateVertexUVs — UV count {} != submesh unique vertex count {}",
			               uvs.size(), uniqueVerts.size());
			return;
		}

		for (size_t k = 0; k < uniqueVerts.size(); ++k)
			rec->vertices[uniqueVerts[k]].TexCoords = uvs[k];

		IRenderFactory* f = Application::Get().Render().GetRenderFactory();
		if (!f) { CHE_CORE_ERROR("MeshLoader::UpdateVertexUVs — no render factory"); return; }

		// Update the contiguous range covering all touched vertices.
		const uint32_t minV = uniqueVerts.front();
		const uint32_t maxV = uniqueVerts.back();
		const uint64_t offset = static_cast<uint64_t>(minV) * sizeof(Vertex);
		const uint64_t bytes  = static_cast<uint64_t>(maxV - minV + 1) * sizeof(Vertex);
		std::span<const std::byte> data{
		    reinterpret_cast<const std::byte*>(rec->vertices.data() + minV),
		    static_cast<size_t>(bytes)
		};
		f->UpdateBuffer(rec->vb, data, offset);
	}

	void MeshLoader::DestroyRecord(MeshGpuRecordHandle rh, MeshGpuRecord* r)
	{
		auto it = m_HashBuckets.find(r->contentHash);
		if (it != m_HashBuckets.end())
		{
			auto& bucket = it->second;
			bucket.erase(std::remove(bucket.begin(), bucket.end(), rh), bucket.end());
			if (bucket.empty())
				m_HashBuckets.erase(it);
		}

		IRenderFactory* f = Application::Get().Render().GetRenderFactory();
		if (f)
		{
			if (r->vb.IsValid()) f->Delete(r->vb);
			if (r->ib.IsValid()) f->Delete(r->ib);
		}

		m_Records.Remove(rh);
	}

	void MeshLoader::Shutdown()
	{
		m_Meshes.Clear();

		IRenderFactory* f = Application::Get().Render().GetRenderFactory();
		m_Records.ForEachOccupied([&](MeshGpuRecord* r) {
			if (!f) return;
			if (r->vb.IsValid()) f->Delete(r->vb);
			if (r->ib.IsValid()) f->Delete(r->ib);
		});
		m_Records.Clear();
		m_HashBuckets.clear();
	}
}
