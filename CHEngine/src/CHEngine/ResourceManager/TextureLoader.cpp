#include "TextureLoader.h"

#include "ResourceManager.h"
#include "CHEngine/Application.h"

namespace CHEngine
{
	TextureHandle TextureLoader::Load(const std::filesystem::path& filepath)
	{
		CHE_CORE_ASSERT(filepath.is_absolute(), "TextureLoader::Load - filepath must be absolute");

		if (auto handle = m_HandlesBimap.left.find(filepath); handle != m_HandlesBimap.left.end())
			return handle->second;

		auto handle = (Handle<void>)Application::Get().Render().CreateTextureFromFile(filepath.string());
		m_HandlesBimap.insert({ filepath, handle });
		return handle;
	}

	void TextureLoader::Unload(TextureHandle handle)
	{
		if (auto it = m_HandlesBimap.right.find(handle); it != m_HandlesBimap.right.end())
			m_HandlesBimap.right.erase(it);
		Application::Get().Render().DestroyTexture(handle);
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
