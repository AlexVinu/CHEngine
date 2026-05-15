#pragma once

#include <Core.h>
#include "Memory/Handle.h"

#include <string>

#include <boost/bimap.hpp>
#include <boost/bimap/unordered_set_of.hpp>

namespace CHEngine
{
	template<typename Key, typename Value>
	using ResourceBimap = boost::bimap<
		boost::bimaps::unordered_set_of<Key>,
		boost::bimaps::unordered_set_of<Value>
	>;

	enum class ELoaderResourceType {
		Texture,
		Shader,
		Mesh,
		Model,
		Prefab,
		AssetPack,
		Sound,
		SIZE
	};

	// Interface for loaders which provide load/unload/caching external resources
	class IResourceLoader {
	public:

		virtual ~IResourceLoader() = default;

		virtual ELoaderResourceType GetResourceType() const = 0;

		virtual const std::string GetName() const = 0;

		virtual bool Initialize() = 0;
		virtual void Shutdown() = 0;

		virtual void Update(float deltaTime) {}

		virtual size_t GetMemoryUsage() const = 0;
		virtual void SetMemoryBudget(size_t bytes) = 0;
		virtual void EvictToBudget(size_t targetBytes) = 0;

		virtual size_t GetCachedCount() const = 0;
	};
}
