#include "ShaderLoader.h"

#include "CHEngine/Render/RenderFacade.h"

namespace CHEngine
{
	ShaderHandle ShaderLoader::Load(const std::string& name, const std::string& filepath)
	{
		if (auto handle = m_HandlesBimap.left.find(filepath); handle != m_HandlesBimap.left.end())
			return handle->second;

		auto handle = RenderFacade::CreateShaderFromFile(String(name), String(filepath));
		m_HandlesBimap.insert({ filepath, handle });
		return handle;
	}

	void ShaderLoader::Unload(ShaderHandle handle)
	{
		auto it = m_HandlesBimap.right.find(handle);
		if (it != m_HandlesBimap.right.end())
			m_HandlesBimap.right.erase(it);

		RenderFacade::DestroyShader(handle);
	}

	size_t ShaderLoader::GetMemoryUsage() const
	{
		throw std::logic_error("The method or operation is not implemented.");
	}

	void ShaderLoader::SetMemoryBudget(size_t bytes)
	{
		throw std::logic_error("The method or operation is not implemented.");
	}

	void ShaderLoader::EvictToBudget(size_t targetBytes)
	{
		throw std::logic_error("The method or operation is not implemented.");
	}

	size_t ShaderLoader::GetCachedCount() const
	{
		throw std::logic_error("The method or operation is not implemented.");
	}

}