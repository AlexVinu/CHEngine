#pragma once

#include "CHEngine/Scene/Entity.h"
#include "CHEngine/Scene/Scene.h"

#include <functional>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace CHEngine
{
    class World;

    using DeferredEntityHandle = uint32_t;

    struct DeferredEntityTarget
    {
        enum class Type
        {
            EntityHandle,
            DeferredEntityHandle,
            UUID
        };

        Type TargetType = Type::EntityHandle;
        EntityHandle Entity{};
        DeferredEntityHandle DeferredEntity = 0;
        UUID EntityUUID{};

        static DeferredEntityTarget FromEntityHandle(EntityHandle entity_handle)
        {
            DeferredEntityTarget target{};
            target.TargetType = Type::EntityHandle;
            target.Entity = entity_handle;
            return target;
        }

        static DeferredEntityTarget FromDeferredHandle(DeferredEntityHandle deferred_entity_handle)
        {
            DeferredEntityTarget target{};
            target.TargetType = Type::DeferredEntityHandle;
            target.DeferredEntity = deferred_entity_handle;
            return target;
        }

        static DeferredEntityTarget FromUUID(const UUID& uuid)
        {
            DeferredEntityTarget target{};
            target.TargetType = Type::UUID;
            target.EntityUUID = uuid;
            return target;
        }
    };

    struct CreateEntityCommand
    {
        DeferredEntityHandle DeferredEntity = 0;
        std::string Name = "Object";
        std::optional<UUID> ExplicitUUID;
    };

    struct DestroyEntityCommand
    {
        DeferredEntityTarget Target;
    };

    struct AddComponentCommand
    {
        DeferredEntityTarget Target;
        std::function<void(Ref<Scene>, EntityHandle)> Apply;
    };

    struct RemoveComponentCommand
    {
        DeferredEntityTarget Target;
        std::function<void(Ref<Scene>, EntityHandle)> Apply;
    };

    struct CustomCommand
    {
        std::function<void(Ref<Scene>)> Callback;
    };

    using DeferredCommandRecord = std::variant<CreateEntityCommand, DestroyEntityCommand, AddComponentCommand, RemoveComponentCommand, CustomCommand>;

    // Provides deferred structural operations for a scene
    class CHENGINE_API DeferredOps {
        friend class World;

    public:
        using FnOnScene = std::function<void(Ref<Scene>)>;

        // Create / destroy operations.
        DeferredEntityHandle CreateEntity(const std::string& name = "Object");
        DeferredEntityHandle CreateEntityWithUUID(const std::string& name, const UUID& uuid);
        void DestroyEntity(EntityHandle entity_handle);
        void DestroyEntity(DeferredEntityHandle deferred_entity_handle);
        void DestroyEntityByUUID(const UUID& uuid);

        template<typename T, typename... Args>
        void AddComponent(DeferredEntityHandle deferred_entity_handle, Args&&... args)
        {
            DeferredEntityTarget target = DeferredEntityTarget::FromDeferredHandle(deferred_entity_handle);
            QueueAddComponent<T>(target, std::forward<Args>(args)...);
        }

        template<typename T, typename... Args>
        void AddComponent(EntityHandle entity_handle, Args&&... args)
        {
            DeferredEntityTarget target = DeferredEntityTarget::FromEntityHandle(entity_handle);
            QueueAddComponent<T>(target, std::forward<Args>(args)...);
        }

        template<typename T, typename... Args>
        void AddComponentByUUID(const UUID& uuid, Args&&... args)
        {
            DeferredEntityTarget target = DeferredEntityTarget::FromUUID(uuid);
            QueueAddComponent<T>(target, std::forward<Args>(args)...);
        }

        template<typename T>
        void RemoveComponent(DeferredEntityHandle deferred_entity_handle)
        {
            DeferredEntityTarget target = DeferredEntityTarget::FromDeferredHandle(deferred_entity_handle);
            QueueRemoveComponent<T>(target);
        }

        template<typename T>
        void RemoveComponent(EntityHandle entity_handle)
        {
            DeferredEntityTarget target = DeferredEntityTarget::FromEntityHandle(entity_handle);
            QueueRemoveComponent<T>(target);
        }

        template<typename T>
        void RemoveComponentByUUID(const UUID& uuid)
        {
            DeferredEntityTarget target = DeferredEntityTarget::FromUUID(uuid);
            QueueRemoveComponent<T>(target);
        }

        void Clear();

        // Manual work with deferred operations
        template<class Fn>
        void Enqueue(Fn&& f) {
            CustomCommand command{};
            command.Callback = std::forward<Fn>(f);
            m_Commands.emplace_back(std::move(command));
        }

    private:
        template<typename T, typename... Args>
        void QueueAddComponent(const DeferredEntityTarget& target, Args&&... args)
        {
            AddComponentCommand command{};
            command.Target = target;
            auto tuple_args = std::make_tuple(std::forward<Args>(args)...);
            command.Apply = [tuple_args = std::move(tuple_args)](Ref<Scene> scene, EntityHandle entity_handle) mutable
            {
                Entity* entity = scene->TryGetEntity(entity_handle);
                if (!entity || entity->HasComponent<T>())
                    return;

                std::apply(
                    [&](auto&&... unpacked_args)
                    {
                        entity->AddComponent<T>(std::forward<decltype(unpacked_args)>(unpacked_args)...);
                    },
                    std::move(tuple_args));
            };
            m_Commands.emplace_back(std::move(command));
        }

        template<typename T>
        void QueueRemoveComponent(const DeferredEntityTarget& target)
        {
            RemoveComponentCommand command{};
            command.Target = target;
            command.Apply = [](Ref<Scene> scene, EntityHandle entity_handle)
            {
                Entity* entity = scene->TryGetEntity(entity_handle);
                if (!entity || !entity->HasComponent<T>())
                    return;
                entity->RemoveComponent<T>();
            };
            m_Commands.emplace_back(std::move(command));
        }

        EntityHandle ResolveTarget(Ref<Scene> scene, const DeferredEntityTarget& target, const std::unordered_map<DeferredEntityHandle, EntityHandle>& created_handles) const;
        static uint64_t HandleToKey(EntityHandle entity_handle);

        void Flush(Ref<Scene> scene);

    private:
        std::vector<DeferredCommandRecord> m_Commands;
        DeferredEntityHandle m_NextDeferredEntity = 1;
    };
}