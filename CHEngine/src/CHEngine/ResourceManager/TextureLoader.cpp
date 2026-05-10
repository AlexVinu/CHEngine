#include "TextureLoader.h"

#include "CHEngine/Render/RenderFacade.h"

namespace CHEngine
{
	TextureHandle TextureLoader::Load(const std::string& path)
	{
		if (auto handle = m_HandlesBimap.left.find(path); handle != m_HandlesBimap.left.end())
			return handle->second;

		auto handle = (Handle<void>)RenderFacade::CreateTextureFromFile(path);
		m_HandlesBimap.insert({ path, handle });
		return handle;
	}

	void TextureLoader::Unload(TextureHandle handle)
	{
		if (auto it = m_HandlesBimap.right.find(handle); it != m_HandlesBimap.right.end())
			m_HandlesBimap.right.erase(it);
		RenderFacade::DestroyTexture(handle);
	}

	size_t TextureLoader::GetMemoryUsage() const
	{
		throw std::logic_error("The method or operation is not implemented.");
	}

	void TextureLoader::SetMemoryBudget(size_t bytes)
	{
		throw std::logic_error("The method or operation is not implemented.");
	}

	void TextureLoader::EvictToBudget(size_t targetBytes)
	{
		throw std::logic_error("The method or operation is not implemented.");
	}

	size_t TextureLoader::GetCachedCount() const
	{
		throw std::logic_error("The method or operation is not implemented.");
	}

}