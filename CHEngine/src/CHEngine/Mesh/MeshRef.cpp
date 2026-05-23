#include "chepch.h"
#include "MeshRef.h"

#include "CHEngine/Application.h"

namespace CHEngine {

	static MeshLoader* GetLoader()
	{
		return Application::Get().Resources().GetMeshLoader();
	}

	MeshRef::~MeshRef()
	{
		if (m_Handle.IsValid())
			if (auto* l = GetLoader()) l->Release(m_Handle);
	}

	MeshRef::MeshRef(const MeshRef& other)
	{
		if (other.m_Handle.IsValid())
			if (auto* l = GetLoader()) m_Handle = l->AddRef(other.m_Handle);
	}

	MeshRef& MeshRef::operator=(const MeshRef& other)
	{
		if (this == &other) return *this;
		if (m_Handle.IsValid())
			if (auto* l = GetLoader()) l->Release(m_Handle);
		m_Handle = {};
		if (other.m_Handle.IsValid())
			if (auto* l = GetLoader()) m_Handle = l->AddRef(other.m_Handle);
		return *this;
	}

	MeshRef& MeshRef::operator=(MeshRef&& other) noexcept
	{
		if (this == &other) return *this;
		if (m_Handle.IsValid())
			if (auto* l = GetLoader()) l->Release(m_Handle);
		m_Handle = other.m_Handle;
		other.m_Handle = {};
		return *this;
	}

	Mesh* MeshRef::operator->()
	{
		auto* l = GetLoader();
		return l ? l->Get(m_Handle) : nullptr;
	}

	const Mesh* MeshRef::operator->() const
	{
		const auto* l = GetLoader();
		return l ? l->Get(m_Handle) : nullptr;
	}

}
