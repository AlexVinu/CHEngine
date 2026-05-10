#include "ModelLoader.h"

#include "Log/Log.h"

#include <algorithm>
#include <filesystem>
#include <stdexcept>

namespace CHEngine
{
	ModelHandle ModelLoader::Load(const std::string& filepath, ShaderHandle meshShader)
	{
		CHE_CORE_ASSERT(meshShader.IsValid(), "ModelLoader::Load - meshShader must be valid");

		if (auto it = m_HandlesBimap.left.find(filepath); it != m_HandlesBimap.left.end())
			return it->second;

		std::string ext = std::filesystem::path(filepath).extension().string();
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
