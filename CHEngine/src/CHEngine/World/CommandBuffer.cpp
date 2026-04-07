#include "chepch.h"
#include "CommandBuffer.h"

namespace CHEngine
{
    void CommandBuffer::createEntity(const std::string& name)
    {
        m_Commands.emplace_back(CreateEntityCommand{ name, std::nullopt });
    }

    void CommandBuffer::createEntityWithUUID(const std::string& name, const UUID& uuid)
    {
        m_Commands.emplace_back(CreateEntityCommand{ name, uuid });
    }

    void CommandBuffer::destroyEntity(EntityHandle handle)
    {
        m_Commands.emplace_back(DestroyEntityCommand{ handle, std::nullopt });
    }

    void CommandBuffer::destroyEntityByUUID(const UUID& uuid)
    {
        m_Commands.emplace_back(DestroyEntityCommand{ std::nullopt, uuid });
    }

    void CommandBuffer::collectPendingDestroyHandles(const Scene& scene, std::vector<EntityHandle>& outHandles) const
    {
        for (const Command& command : m_Commands)
        {
            const auto* destroy = std::get_if<DestroyEntityCommand>(&command);
            if (!destroy)
                continue;

            if (destroy->Handle.has_value())
            {
                outHandles.push_back(*destroy->Handle);
                continue;
            }

            if (!destroy->EntityUUID.has_value())
                continue;

            const EntityHandle handle = scene.TryGetEntityHandleByUUID(*destroy->EntityUUID);
            if (scene.IsEntityHandleValid(handle))
                outHandles.push_back(handle);
        }
    }

    void CommandBuffer::flush(Scene& scene)
    {
        for (const Command& command : m_Commands) {
            if (const auto* create = std::get_if<CreateEntityCommand>(&command)) {
                if (create->ExplicitUUID.has_value())
                    scene.CreateEntity(create->Name, *create->ExplicitUUID);
                else
                    scene.CreateEntity(create->Name);
                continue;
            }

            const auto* destroy = std::get_if<DestroyEntityCommand>(&command);
            if (!destroy)
                continue;

            if (destroy->Handle.has_value()) {
                if (scene.IsEntityHandleValid(*destroy->Handle))
                    scene.DestroyEntity(*destroy->Handle);
                continue;
            }

            if (!destroy->EntityUUID.has_value())
                continue;

            const EntityHandle handle = scene.TryGetEntityHandleByUUID(*destroy->EntityUUID);
            if (scene.IsEntityHandleValid(handle))
                scene.DestroyEntity(handle);
        }

        m_Commands.clear();
    }

    void CommandBuffer::clear()
    {
        m_Commands.clear();
    }
}
