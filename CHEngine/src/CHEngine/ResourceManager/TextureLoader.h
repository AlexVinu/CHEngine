#pragma once

#include "IResourceLoader.h"
#include "Render/Handles.h"
#include <filesystem>

namespace CHEngine
{
	class CHENGINE_API TextureLoader : public IResourceLoader
	{

	public:
		ELoaderResourceType GetResourceType() const override { return ELoaderResourceType::Texture; };
		const std::string GetName() const override { return "Texture Loader"; };

		bool Initialize() override { return true; }
		void Shutdown() override {}

		TextureHandle Load(const std::filesystem::path& path);
		void Unload(TextureHandle handle);

		size_t GetMemoryUsage() const override;
		void SetMemoryBudget(size_t bytes) override;
		void EvictToBudget(size_t targetBytes) override;
		size_t GetCachedCount() const override;

	private:
		ResourceBimap<std::filesystem::path, TextureHandle> m_HandlesBimap;
	};
}
