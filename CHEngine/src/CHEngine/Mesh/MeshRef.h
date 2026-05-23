#pragma once

#include <Core.h>
#include "CHEngine/ResourceManager/MeshLoader.h"

namespace CHEngine {

	// RAII handle for a Mesh stored in MeshLoader.
	// Copy calls MeshLoader::AddRef (new Mesh, shared GpuRecord).
	// Destruction calls MeshLoader::Release.
	class CHENGINE_API MeshRef
	{
	public:
		MeshRef() = default;
		explicit MeshRef(MeshHandle h) : m_Handle(h) {}

		~MeshRef();

		MeshRef(const MeshRef& other);
		MeshRef& operator=(const MeshRef& other);

		MeshRef(MeshRef&& other) noexcept : m_Handle(other.m_Handle) { other.m_Handle = {}; }
		MeshRef& operator=(MeshRef&& other) noexcept;

		MeshHandle Handle() const { return m_Handle; }
		bool       IsValid() const { return m_Handle.IsValid(); }

		// Pointer-style access to the underlying Mesh.
		Mesh*       operator->();
		const Mesh* operator->() const;

	private:
		MeshHandle m_Handle;
	};

}
