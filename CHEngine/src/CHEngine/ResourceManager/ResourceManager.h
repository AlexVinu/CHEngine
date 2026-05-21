#pragma once

#include "IResourceLoader.h"
#include "ShaderLoader.h"
#include "TextureLoader.h"
#include "ModelLoader.h"
#include "MeshLoader.h"

#include "CHEngine/Mesh/Mesh.h"

#include <array>
#include <filesystem>
#include <string>
#include <string_view>

namespace CHEngine {

	// Centralized resource loading with caching and memory control.
	// Uses fullpaths 
	class CHENGINE_API ResourceManager
	{
	public:
		ResourceManager();
		~ResourceManager();

		template<class Handle, class... Args>
		Handle Load(Args&&... args)
		{
			if constexpr (std::is_same_v<Handle, ShaderHandle>)
				return GetShaderLoader()->Load(std::forward<Args>(args)...);
			else if constexpr (std::is_same_v<Handle, TextureHandle>)
				return GetTextureLoader()->Load(std::forward<Args>(args)...);
			else if constexpr (std::is_same_v<Handle, ModelHandle>)
				return GetModelLoader()->Load(std::forward<Args>(args)...);
			else if constexpr (std::is_same_v<Handle, MeshHandle>)
				return GetMeshLoader()->GetOrCreate(std::forward<Args>(args)...);
			else
				static_assert(!sizeof(Handle), "Unsupported resource type for ResourceManager::Load");
		}

		template<class Handle>
		void Unload(Handle handle)
		{
			if constexpr (std::is_same_v<Handle, ShaderHandle>)
				GetShaderLoader()->Unload(handle);
			else if constexpr (std::is_same_v<Handle, TextureHandle>)
				GetTextureLoader()->Unload(handle);
			else if constexpr (std::is_same_v<Handle, ModelHandle>)
				GetModelLoader()->Unload(handle);
			else if constexpr (std::is_same_v<Handle, MeshHandle>)
				GetMeshLoader()->Release(handle);
			else
				static_assert(!sizeof(Handle), "Unsupported resource type for ResourceManager::Unload");
		}

		const LoadedModel* GetModel(ModelHandle h) const { return GetModelLoader()->Get(h); }

		// Returns an invalid Mesh for unknown URIs.
		Mesh LoadPrimitiveMesh(std::string_view uri);

		static bool IsPrimitiveUri(std::string_view uri) { return uri.starts_with(":primitive:"); }

	private:
		static constexpr size_t LOADERS_COUNT = static_cast<size_t>(ELoaderResourceType::SIZE);
		std::array<Scope<IResourceLoader>, LOADERS_COUNT> m_ResourceLoaders;

		ShaderLoader*  GetShaderLoader()  const { return static_cast<ShaderLoader*> (m_ResourceLoaders[static_cast<size_t>(ELoaderResourceType::Shader)].get()); }
		TextureLoader* GetTextureLoader() const { return static_cast<TextureLoader*>(m_ResourceLoaders[static_cast<size_t>(ELoaderResourceType::Texture)].get()); }
		ModelLoader*   GetModelLoader()   const { return static_cast<ModelLoader*>  (m_ResourceLoaders[static_cast<size_t>(ELoaderResourceType::Mesh)].get()); }
		MeshLoader*    GetMeshLoader()    { return &MeshLoader::Instance(); }
	};

}
