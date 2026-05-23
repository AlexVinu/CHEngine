#pragma once

#include "IResourceLoader.h"
#include "CHEngine/Mesh/Mesh.h"
#include "CHEngine/Mesh/Vertex.h"
#include "Memory/HandlePool.h"
#include "Render/Handles.h"

#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace CHEngine
{
	struct MeshHandleTag {};
	using MeshHandle = Handle<MeshHandleTag>;

	// Shared GPU data for a unique (vertices, indices) pair.
	// Referenced by multiple Mesh objects; freed when refcount reaches zero.
	struct MeshGpuRecord
	{
		BufferHandle          vb{};
		BufferHandle          ib{};
		uint32_t              indexCount  = 0;
		uint64_t              contentHash = 0;
		uint32_t              refcount    = 0;
		std::vector<Vertex>   vertices;
		std::vector<uint32_t> indices;
	};

	// Content-addressed cache of GPU mesh buffers.
	// Identical (vertices, indices) pairs share one MeshGpuRecord.
	// Not a singleton — single instance owned by ResourceManager.
	class CHENGINE_API MeshLoader : public IResourceLoader
	{
	public:
		ELoaderResourceType GetResourceType() const override { return ELoaderResourceType::Mesh; }
		const std::string   GetName()         const override { return "MeshLoader"; }

		bool Initialize() override { return true; }
		void Shutdown()   override;

		// Returns a new MeshHandle backed by a (possibly shared) GpuRecord.
		MeshHandle GetOrCreate(const std::vector<Vertex>& vertices,
		                       const std::vector<uint32_t>& indices,
		                       Ref<MaterialInstance> mat = nullptr);

		// Creates a new MeshHandle sharing the same GpuRecord (for component copy).
		MeshHandle AddRef(MeshHandle h);

		// Releases the Mesh; destroys the GpuRecord + GPU buffers when refcount hits zero.
		void Release(MeshHandle h);

		Mesh*       Get(MeshHandle h);
		const Mesh* Get(MeshHandle h) const;

		// Returns the underlying GpuRecord for vertex/index CPU-side access.
		const MeshGpuRecord* GetGpuRecord(MeshHandle h) const;

		// Replaces material on this Mesh only (GpuRecord stays shared).
		void SetMaterial(MeshHandle h, Ref<MaterialInstance> mat);

		// Updates UV data in the shared GpuRecord (visible to all Mesh sharing it).
		void UpdateVertexUVs(MeshHandle h, std::span<const glm::vec2> uvs);

		size_t GetMemoryUsage()  const override { return 0; }
		void   SetMemoryBudget(size_t) override {}
		void   EvictToBudget(size_t)   override {}
		size_t GetCachedCount()  const override { return m_Meshes.Count(); }

	private:
		void DestroyRecord(MeshGpuRecordHandle rh, MeshGpuRecord* r);

		HandlePool<MeshGpuRecord, MeshGpuRecordTag>                    m_Records{ [](MeshGpuRecord* p){ delete p; } };
		HandlePool<Mesh,          MeshHandleTag>                       m_Meshes { [](Mesh* p)         { delete p; } };
		std::unordered_map<uint64_t, std::vector<MeshGpuRecordHandle>> m_HashBuckets;
	};
}
