#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "CHEngine/Scene/Scene.h"

namespace CHEngine
{
	class CommandBuffer
	{
    public:
        void createEntity(const std::string& name = "Object");
        void createEntityWithUUID(const std::string& name, const UUID& uuid);
        void destroyEntity(EntityHandle handle);
        void destroyEntityByUUID(const UUID& uuid);
        void collectPendingDestroyHandles(const Scene& scene, std::vector<EntityHandle>& outHandles) const;

        void flush(Scene& scene);
        void clear();
        bool empty() const { return m_Commands.empty(); }
        size_t size() const { return m_Commands.size(); }

    private:
        struct CreateEntityCommand {
            std::string Name = "Object";
            std::optional<UUID> ExplicitUUID;
        };

        struct DestroyEntityCommand {
            std::optional<EntityHandle> Handle;
            std::optional<UUID> EntityUUID;
        };

        using Command = std::variant<CreateEntityCommand, DestroyEntityCommand>;
        std::vector<Command> m_Commands;
	};
}