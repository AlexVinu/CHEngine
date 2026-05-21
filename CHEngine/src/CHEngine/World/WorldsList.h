#pragma once

#include <Core.h>
#include "CheStl/MemoryTypes.h"

#include <string>
#include <string_view>
#include <vector>

namespace CHEngine {

class World;

class CHENGINE_API WorldsList {
public:
    WorldsList() = default;
    WorldsList(const WorldsList&) = delete;
    WorldsList& operator=(const WorldsList&) = delete;
    WorldsList(WorldsList&&) = default;
    WorldsList& operator=(WorldsList&&) = default;

    // Invokes World constructor
	template<typename... Args>
	World* EmplaceBack(Args&&... args)
    {
		auto scope = MakeScope<World>(std::forward<Args>(args)..., this);
		World* result = scope.get();

		m_Items.push_back(std::move(scope));
		return result;
    }
    // Not invokes World constructor
	World* PushBack(World* world)
	{
		m_Items.emplace_back(world); 
		return world;
	}

    void Erase(size_t index);
    void Erase(World* world);

    // Returns nullptr if no World with this name is registered.
    World* TryGet(std::string_view name) const;
    World* GetForIndex(size_t index) const;
    // Return -1 if world not found
    int GetIndex(const World* world) const;

    void Swap(size_t i, size_t j)
    {
        if (i < m_Items.size() && j < m_Items.size())
            m_Items[i].swap(m_Items[j]);
    }

    bool IsEmpty() const { return m_Items.empty(); }
    size_t Size() const { return m_Items.size(); }

    template<typename Fn>
    void ForEach(Fn&& fn) const
    {
        for (auto& w : m_Items)
            fn(*w);
    }

private:
    std::vector<Scope<World>> m_Items;
};

} // namespace CHEngine
