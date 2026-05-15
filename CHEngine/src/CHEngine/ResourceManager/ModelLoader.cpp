#include "ModelLoader.h"

#include "ResourceManager.h"
#include "Log/Log.h"

#include <algorithm>
#include <filesystem>
#include <stdexcept>

namespace CHEngine
{
	ModelHandle ModelLoader::Load(const std::filesystem::path& filepath, ShaderHandle meshShader)
	{
		CHE_CORE_ASSERT(meshShader.IsValid(), "ModelLoader::Load - meshShader must be valid");
		CHE_CORE_ASSERT(filepath.is_absolute(), "ModelLoader::Load - filepath must be absolute");

		// Cache key uses the input path verbatim; resolution happens just before disk I/O.
		if (auto it = m_HandlesBimap.left.find(filepath); it != m_HandlesBimap.left.end())
			return it->second;

		std::string ext = filepath.extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

		ModelHandle handle;
		if (ext == ".obj")
			handle = LoadOBJ(filepath, meshShader);
		else if (ext == ".glb" || ext == ".gltf")
			handle = LoadGLTF(filepath, meshShader);
		else
		{
			CHE_CORE_ERROR("ModelLoader: unsupported file format: {}", ext);
			return {};
		}

		if (handle.IsValid())
			m_HandlesBimap.insert({ filepath, handle });

		return handle;
	}

	void ModelLoader::Unload(ModelHandle handle)
	{
		if (auto it = m_HandlesBimap.right.find(handle); it != m_HandlesBimap.right.end())
			m_HandlesBimap.right.erase(it);

		m_Models.Remove(handle);
	}

	size_t ModelLoader::GetMemoryUsage() const
	{
		throw std::logic_error("The method or operation is not implemented.");
	}

	void ModelLoader::SetMemoryBudget(size_t bytes)
	{
		throw std::logic_error("The method or operation is not implemented.");
	}

	void ModelLoader::EvictToBudget(size_t targetBytes)
	{
		throw std::logic_error("The method or operation is not implemented.");
	}

	size_t ModelLoader::GetCachedCount() const
	{
		return m_Models.Count();
	}
}
