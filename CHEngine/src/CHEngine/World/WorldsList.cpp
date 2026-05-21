#include "chepch.h"
#include "WorldsList.h"
#include "World.h"

#include <algorithm>

namespace CHEngine {

void WorldsList::Erase(size_t index)
{
    CHE_CORE_ASSERT(index < m_Items.size(), "WORLD LIST: Given wrong index");
	m_Items.erase(m_Items.begin() + index);
}

void WorldsList::Erase(World* world)
{
	m_Items.erase(
		std::remove_if(m_Items.begin(), m_Items.end(),
			[world](const Scope<World>& ptr) { return ptr.get() == world; }
		),
		m_Items.end()
	);
}

World* WorldsList::TryGet(std::string_view name) const
{
    for (auto& w : m_Items)
        if (w->GetWorldName() == name)
            return w.get();
    return nullptr;
}


World* WorldsList::GetForIndex(size_t index) const
{
	CHE_CORE_ASSERT(index < m_Items.size(), "WORLD LIST: Given wrong index");
	return m_Items[index].get();
}

int WorldsList::GetIndex(const World* world) const
{
	for (size_t i = 0; i < m_Items.size(); ++i)
	{
		if (m_Items[i].get() == world)
			return i;
	}
	return -1;
}

} // namespace CHEngine
