#pragma once

#include "IResourceLoader.h"
#include "CHEngine/Mesh/Vertex.h"
#include "Memory/HandlePool.h"
#include "Render/Handles.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace CHEngine
{
	struct MeshHandleTag {};
	using MeshHandle = Handle<MeshHandleTag>;

	struct MeshGpuRecord
	{
		BufferHandle vb{};
		BufferHandle ib{};
		uint32_t     indexCount = 0;
		uint64_t     contentHash = 0;
		uint32_t     refcount = 0;

		std::vector<Vertex>   vertices;
		std::vector<uint32_t> indices;
	};

	// Content-addressed cache of GPU mesh buffers.
	// Identical (vertices, indices) → shared VB/IB. Refcounted; entries freed when
	// the last Mesh referencing them is destroyed.
	class MeshLoader : public IResourceLoader
	{
	public:
		ELoaderResourceType GetResourceType() const override { return ELoaderResourceType::Mesh; }
		const std::string GetName() const override { return "MeshLoader"; }

		bool Initialize() override { return true; }
		void Shutdown() override;

		// Returns (and AddRef's) a handle to a record matching the data; uploads to GPU on first use.
		MeshHandle GetOrCreate(const std::vector<Vertex>& vertices,
		                       const std::vector<uint32_t>& indices);

		void AddRef(MeshHandle h);
		void Release(MeshHandle h);

		const MeshGpuRecord* Get(MeshHandle h) const;

		size_t GetMemoryUsage() const override { return 0; }
		void   SetMemoryBudget(size_t) override {}
		void   EvictToBudget(size_t) override {}

		size_t GetCachedCount() const override { return m_Records.Count(); }

		// Process-wide accessor used by Mesh::Build / ~Mesh.
		static MeshLoader& Instance();

	private:
		void DeleteRecord(MeshGpuRecord* r);

		HandlePool<MeshGpuRecord, MeshHandleTag>              m_Records{ [](MeshGpuRecord* p) { delete p; } };
		std::unordered_map<uint64_t, std::vector<MeshHandle>> m_HashBuckets;
	};
}
