#pragma once

#include "IResourceLoader.h"
#include "Render/Handles.h"
#include <filesystem>

namespace CHEngine
{
	class CHENGINE_API ShaderLoader : public IResourceLoader
	{
	public:
		ELoaderResourceType GetResourceType() const override { return ELoaderResourceType::Shader; };
		const std::string GetName() const override { return "ShaderLoader"; };

		bool Initialize() override { return true; }
		void Shutdown() override {}

		// Here first - name, second - slang shader
		ShaderHandle Load(const std::string& name, const std::filesystem::path& filepath);
		void Unload(ShaderHandle handle);

		size_t GetMemoryUsage() const override;
		void SetMemoryBudget(size_t bytes) override;
		void EvictToBudget(size_t targetBytes) override;

		size_t GetCachedCount() const override;
	private:
		ResourceBimap<std::filesystem::path, ShaderHandle> m_HandlesBimap;
	};
}
