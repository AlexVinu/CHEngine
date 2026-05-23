#include "ResourceManager.h"

#include "CHEngine/Mesh/PrimitiveMeshFactory.h"
#include "CHEngine/Utils/AppPaths.h"
#include "Log/Log.h"

namespace CHEngine
{

	ResourceManager::ResourceManager()
	{
		m_ResourceLoaders[static_cast<size_t>(ELoaderResourceType::Texture)] = std::make_unique<TextureLoader>();
		m_ResourceLoaders[static_cast<size_t>(ELoaderResourceType::Shader)]  = std::make_unique<ShaderLoader>();
		m_ResourceLoaders[static_cast<size_t>(ELoaderResourceType::Model)]   = std::make_unique<ModelLoader>();
		m_ResourceLoaders[static_cast<size_t>(ELoaderResourceType::Mesh)]    = std::make_unique<MeshLoader>();

		for (auto& loader : m_ResourceLoaders)
		{
			if (loader)
				loader->Initialize();
		}
	}

	ResourceManager::~ResourceManager()
	{
		for (auto& loader : m_ResourceLoaders)
		{
			if (loader)
				loader->Shutdown();
		}
	}

	MeshRef ResourceManager::LoadPrimitiveMesh(std::string_view uri)
	{
		if (uri == ":primitive:cube")
			return PrimitiveMeshFactory::CreateCube(1.0f, { 0.8f, 0.8f, 0.8f });
		if (uri == ":primitive:sphere")
			return PrimitiveMeshFactory::CreateSphereImpostor({ 0.6f, 0.7f, 0.9f });
		return {};
	}
}
