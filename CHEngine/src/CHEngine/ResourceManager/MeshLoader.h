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
	// Single VB + single IB host the whole model; submeshes describe draw ranges.
	struct MeshGpuRecord
	{
		BufferHandle           vb{};
		BufferHandle           ib{};
		std::vector<SubMesh>   subMeshes;
		uint64_t               contentHash = 0;
		uint32_t               refcount    = 0;
		std::vector<Vertex>    vertices;
		std::vector<uint32_t>  indices;
	};

	// Content-addressed cache of GPU mesh buffers.
	// Identical (vertices, indices) pairs share one MeshGpuRecord.
	class CHENGINE_API MeshLoader : public IResourceLoader
	{
	public:
		ELoaderResourceType GetResourceType() const override { return ELoaderResourceType::Mesh; }
		const std::string   GetName()         const override { return "MeshLoader"; }

		bool Initialize() override { return true; }
		void Shutdown()   override;

		// Full form: caller provides explicit submesh ranges and matching materials.
		// materials.size() must equal subMeshes.size().
		MeshHandle GetOrCreate(const std::vector<Vertex>& vertices,
		                       const std::vector<uint32_t>& indices,
		                       const std::vector<SubMesh>& subMeshes,
		                       std::vector<Ref<MaterialInstance>> materials);

		// Simple form: single submesh covering all indices, single material.
		MeshHandle GetOrCreate(const std::vector<Vertex>& vertices,
		                       const std::vector<uint32_t>& indices,
		                       Ref<MaterialInstance> mat = nullptr);

		// Creates a new MeshHandle sharing the same GpuRecord, with an independent
		// copy of the source's per-submesh materials.
		MeshHandle AddRef(MeshHandle h);

		// Releases the Mesh; destroys the GpuRecord + GPU buffers when refcount hits zero.
		void Release(MeshHandle h);

		Mesh*       Get(MeshHandle h);
		const Mesh* Get(MeshHandle h) const;

		// Returns the underlying GpuRecord for vertex/index/submesh CPU-side access.
		const MeshGpuRecord* GetGpuRecord(MeshHandle h) const;

		// Replaces material for a single submesh on this Mesh only.
		void SetMaterial(MeshHandle h, uint32_t submeshIndex, Ref<MaterialInstance> mat);

		// Updates UVs for vertices used by the given submesh. uvs.size() must equal
		// the count of unique vertices referenced by that submesh's index range.
		void UpdateVertexUVs(MeshHandle h, uint32_t submeshIndex, std::span<const glm::vec2> uvs);

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
