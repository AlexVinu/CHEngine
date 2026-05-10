#include "chepch.h"
#include "MeshLoader.h"

#include "CHEngine/Render/RenderFacade.h"
#include "Render/Core/RenderTypes.h"
#include "Log/Log.h"

#include <algorithm>
#include <cstring>
#include <span>

namespace CHEngine
{
	namespace
	{
		uint64_t HashBytes(const void* data, size_t size, uint64_t seed = 0xCBF29CE484222325ULL)
		{
			const uint8_t* p = static_cast<const uint8_t*>(data);
			uint64_t h = seed;
			for (size_t i = 0; i < size; ++i)
			{
				h ^= static_cast<uint64_t>(p[i]);
				h *= 0x100000001B3ULL;
			}
			return h;
		}

		uint64_t ComputeContentHash(const std::vector<Vertex>& vertices,
		                            const std::vector<uint32_t>& indices)
		{
			uint64_t h = HashBytes(vertices.data(), vertices.size() * sizeof(Vertex));
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
			if (std::memcmp(r.vertices.data(), vertices.data(), vertices.size() * sizeof(Vertex)) != 0)
				return false;
			if (std::memcmp(r.indices.data(), indices.data(), indices.size() * sizeof(uint32_t)) != 0)
				return false;
			return true;
		}
	}

	MeshLoader& MeshLoader::Instance()
	{
		static MeshLoader s_Instance;
		return s_Instance;
	}

	MeshHandle MeshLoader::GetOrCreate(const std::vector<Vertex>& vertices,
	                                   const std::vector<uint32_t>& indices)
	{
		if (vertices.empty() || indices.empty())
			return {};

		const uint64_t hash = ComputeContentHash(vertices, indices);

		auto it = m_HashBuckets.find(hash);
		if (it != m_HashBuckets.end())
		{
			for (MeshHandle h : it->second)
			{
				MeshGpuRecord* r = m_Records.Get(h);
				if (r && ContentEqual(*r, vertices, indices))
				{
					++r->refcount;
					return h;
				}
			}
		}

		IRenderFactory* f = RenderFacade::GetRenderFactory();
		if (!f) { CHE_CORE_ERROR("MeshLoader::GetOrCreate — no render factory"); return {}; }

		std::vector<float> flat;
		flat.reserve(vertices.size() * 11);
		for (const auto& v : vertices)
		{
			flat.push_back(v.Position.x); flat.push_back(v.Position.y); flat.push_back(v.Position.z);
			flat.push_back(v.Normal.x);   flat.push_back(v.Normal.y);   flat.push_back(v.Normal.z);
			flat.push_back(v.TexCoords.x); flat.push_back(v.TexCoords.y);
			flat.push_back(v.Color.x);    flat.push_back(v.Color.y);    flat.push_back(v.Color.z);
		}

		const uint64_t vbSize = flat.size() * sizeof(float);
		std::span<const std::byte> vbBytes{ reinterpret_cast<const std::byte*>(flat.data()), vbSize };
		BufferHandle vb = f->CreateBuffer(vbSize, BufferUsage::Vertex, MemoryType::GpuOnly,
		                                  vbBytes, String("mesh_vb"));

		const uint64_t ibSize = indices.size() * sizeof(uint32_t);
		std::span<const std::byte> ibBytes{ reinterpret_cast<const std::byte*>(indices.data()), ibSize };
		BufferHandle ib = f->CreateBuffer(ibSize, BufferUsage::Index, MemoryType::GpuOnly,
		                                  ibBytes, String("mesh_ib"));

		auto* rec = new MeshGpuRecord{};
		rec->vb          = vb;
		rec->ib          = ib;
		rec->indexCount  = static_cast<uint32_t>(indices.size());
		rec->contentHash = hash;
		rec->refcount    = 1;
		rec->vertices    = vertices;
		rec->indices     = indices;

		MeshHandle handle = m_Records.Add(rec);
		m_HashBuckets[hash].push_back(handle);
		return handle;
	}

	void MeshLoader::AddRef(MeshHandle h)
	{
		if (!h.IsValid()) return;
		if (MeshGpuRecord* r = m_Records.Get(h))
			++r->refcount;
	}

	void MeshLoader::Release(MeshHandle h)
	{
		if (!h.IsValid()) return;
		MeshGpuRecord* r = m_Records.Get(h);
		if (!r) return;
		if (--r->refcount > 0) return;

		// Remove from bucket; erase bucket if empty.
		auto it = m_HashBuckets.find(r->contentHash);
		if (it != m_HashBuckets.end())
		{
			auto& bucket = it->second;
			bucket.erase(std::remove(bucket.begin(), bucket.end(), h), bucket.end());
			if (bucket.empty())
				m_HashBuckets.erase(it);
		}

		DeleteRecord(r);
		m_Records.Remove(h);
	}

	const MeshGpuRecord* MeshLoader::Get(MeshHandle h) const
	{
		return m_Records.Get(h);
	}

	void MeshLoader::DeleteRecord(MeshGpuRecord* r)
	{
		if (!r) return;
		IRenderFactory* f = RenderFacade::GetRenderFactory();
		if (f)
		{
			if (r->vb.IsValid()) f->Delete(r->vb);
			if (r->ib.IsValid()) f->Delete(r->ib);
		}
		r->vb = {};
		r->ib = {};
	}

	void MeshLoader::Shutdown()
	{
		m_Records.ForEachOccupied([this](MeshGpuRecord* r) { DeleteRecord(r); });
		m_Records.Clear();
		m_HashBuckets.clear();
	}
}
