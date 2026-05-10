#pragma once

#include "IResourceLoader.h"
#include "Render/Handles.h"

namespace CHEngine
{
	class CHENGINE_API TextureLoader : public IResourceLoader
	{

	public:
		ELoaderResourceType GetResourceType() const override { return ELoaderResourceType::Texture; };
		const std::string GetName() const override { return "Texture Loader"; };

		bool Initialize() override { return true; }
		void Shutdown() override {}

		TextureHandle Load(const std::string& path);
		void Unload(TextureHandle handle);

		size_t GetMemoryUsage() const override;
		void SetMemoryBudget(size_t bytes) override;
		void EvictToBudget(size_t targetBytes) override;
		size_t GetCachedCount() const override;

	private:
		ResourceBimap<std::string, TextureHandle> m_HandlesBimap;
	};
}