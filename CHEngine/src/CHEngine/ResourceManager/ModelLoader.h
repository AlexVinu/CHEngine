#pragma once

#include "IResourceLoader.h"
#include "CHEngine/Mesh/LoadedModel.h"
#include "Memory/HandlePool.h"
#include "Render/Handles.h"
#include <filesystem>

namespace CHEngine
{
	struct ModelHandleTag{};
	using ModelHandle = Handle<ModelHandleTag>;

	class CHENGINE_API ModelLoader : public IResourceLoader
	{
	public:
		ELoaderResourceType GetResourceType() const override { return ELoaderResourceType::Mesh; };
		const std::string GetName() const override { return "ModelLoader"; };

		bool Initialize() override { return true; }
		void Shutdown() override {}

		ModelHandle        Load(const std::filesystem::path& filepath, ShaderHandle meshShader);
		void               Unload(ModelHandle handle);
		const LoadedModel* Get(ModelHandle h) const { return m_Models.Get(h); }

		size_t GetMemoryUsage() const override;
		void SetMemoryBudget(size_t bytes) override;
		void EvictToBudget(size_t targetBytes) override;

		size_t GetCachedCount() const override;
	private:
		ModelHandle LoadOBJ(const std::filesystem::path& filepath, ShaderHandle meshShader);
		ModelHandle LoadGLTF(const std::filesystem::path& filepath, ShaderHandle meshShader);

		HandlePool<LoadedModel, ModelHandleTag> m_Models{ [](LoadedModel* p) { delete p; } };
		ResourceBimap<std::filesystem::path, ModelHandle> m_HandlesBimap;
	};
}
