#include "ResourceManager.h"

#include "CHEngine/Project/ProjectManager.h"
#include "CHEngine/Project/Project.h"
#include "CHEngine/Utils/AppPaths.h"
#include "Log/Log.h"

namespace CHEngine
{
	std::filesystem::path ResourceManager::ResolveAssetPath(const std::string& path)
	{
		if (path.empty())
			return {};

		std::filesystem::path p(path);
		if (p.is_absolute())
			return p;

		if (ProjectManager::HasProject())
		{
			Project* proj = ProjectManager::Current();
			// If the relative path already references the assets dir (e.g. "Assets/Models/x.obj"),
			// anchor it to the project root; otherwise place it under <project>/Assets/.
			const std::filesystem::path root = proj->RootDir();
			const std::filesystem::path firstComponent = *p.begin();
			const std::string firstStr = firstComponent.string();
			if (firstStr == "Assets" || firstStr == "Scenes")
				return root / p;
			return proj->AssetsAbsPath() / p;
		}

		// No project — fall back to editor defaults bundled with the executable.
		return AppPaths::EditorAssetsDir() / p;
	}


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
