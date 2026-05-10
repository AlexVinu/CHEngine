#include "ResourceManager.h"

namespace CHEngine
{
	ResourceManager& ResourceManager::Instance()
	{
		static ResourceManager s_Instance;
		return s_Instance;
	}

	ResourceManager::ResourceManager()
	{
		m_ResourceLoaders[static_cast<size_t>(ELoaderResourceType::Texture)] = std::make_unique<TextureLoader>();
		m_ResourceLoaders[static_cast<size_t>(ELoaderResourceType::Shader)]  = std::make_unique<ShaderLoader>();
		m_ResourceLoaders[static_cast<size_t>(ELoaderResourceType::Mesh)]    = std::make_unique<ModelLoader>();

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
}
