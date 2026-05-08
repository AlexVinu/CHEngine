#pragma once

#include "CHEngine/Scene/Entity.h"
#include "CHEngine/Scene/Scene.h"

#include <functional>
#include <optional>
#include <string>
#include <typeindex>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>


namespace CHEngine
{
    class World;

    // Lifetime = one frame
    using DeferredEntityHandle = uint32_t;

    using DeferredOpTarget = std::variant<EntityHandle, DeferredEntityHandle>;

    struct DeferredEntity
    {
        UUID EntityUUID{};
    };

    // Commands
    struct CreateEntityCommand
    {
        DeferredEntityHandle DeferredEntity = 0;
        std::string Name = "Object";
        std::optional<UUID> ExplicitUUID;
    };

    struct DestroyEntityCommand
    {
        EntityHandle Target;
    };

    struct AddComponentCommand
    {
        DeferredOpTarget Target;
        std::function<void(World&, Ref<Scene>, EntityHandle)> Apply;
    };

    struct RemoveComponentCommand
    {
        EntityHandle Target;
        std::function<void(World&, Ref<Scene>, EntityHandle)> Apply;
    };

    struct CustomCommand
    {
        std::function<void(Ref<Scene>)> Callback;
    };

    // Provides deferred structural operations for a scene
    // NOTES: I think there is no sense to delete deferred entity or its components (it literally does not exist)
    class DeferredOps {
        friend class World;

        struct HookTag{};
        
    public:

        ~DeferredOps();
        using HookHandle = Handle<HookTag>;

        using ComponentAddedFn = void(*)(World& world, EntityHandle handle);
        using ComponentRemovedFn = void(*)(World& world, EntityHandle handle);
        using FnOnScene = std::function<void(Ref<Scene>)>;

        // Entity/ DeferredEntity operations 
        DeferredEntityHandle CreateEntity(const std::string& name = "Object");
        DeferredEntityHandle CreateEntityWithUUID(const std::string& name, const UUID& uuid);
        void DestroyEntity(EntityHandle entity_handle);

        template<typename T, typename... Args>
        void AddComponent(DeferredEntityHandle deferred_entity_handle, Args&&... args)
        {
            DeferredOpTarget target = deferred_entity_handle;
            QueueAddComponent<T>(target, std::forward<Args>(args)...);
        }

        template<typename T, typename... Args>
        void AddComponent(EntityHandle entity_handle, Args&&... args)
        {
            DeferredOpTarget target = entity_handle;
            QueueAddComponent<T>(target, std::forward<Args>(args)...);
        }

        template<typename T>
        void RemoveComponent(EntityHandle entity_handle)
        {
            QueueRemoveComponent<T>(entity_handle);
        }

        void Clear();

        // Hooks for components
        template<typename T>
        HookHandle SubscribeOnComponentAdded(ComponentAddedFn fn)
        {
            return SubscribeOnComponentAdded(typeid(T), fn);
        }

        template<typename T>
        HookHandle SubscribeOnComponentRemoved(ComponentRemovedFn fn)
        {
            return SubscribeOnComponentRemoved(typeid(T), fn);
        }

        bool Unsubscribe(HookHandle token);

        // Manual work with deferred operations
        // You must make checks if you deal with entity
        template<class Fn>
        void Enqueue(Fn&& f) {
            CustomCommand command{};
            command.Callback = std::forward<Fn>(f);
            m_CustomCommands.emplace_back(std::move(command));
        }

    private:
        struct ComponentHookBinding
        {
            std::type_index ComponentType = typeid(void);
            uint64_t DedupKey = 0;
            ComponentAddedFn AddedFn = nullptr;
            ComponentRemovedFn RemovedFn = nullptr;
        };

        template<typename T, typename... Args>
        void QueueAddComponent(const DeferredOpTarget& target, Args&&... args)
        {
            AddComponentCommand command{};
            command.Target = target;
            auto tuple_args = std::make_tuple(std::forward<Args>(args)...);
            command.Apply = [this, tuple_args = std::move(tuple_args)](World& world, Ref<Scene> scene, EntityHandle entity_handle) mutable
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

                DispatchOnComponentAdded<T>(world, entity_handle);
            };
            m_AddComponentCommands.emplace_back(std::move(command));
        }

        template<typename T>
        void QueueRemoveComponent(const EntityHandle& target)
        {
            RemoveComponentCommand command{};
            command.Target = target;
            command.Apply = [this](World& world, Ref<Scene> scene, EntityHandle entity_handle)
            {
                Entity* entity = scene->TryGetEntity(entity_handle);
                if (!entity || !entity->HasComponent<T>())
                    return;
                entity->RemoveComponent<T>();
                DispatchOnComponentRemoved<T>(world, entity_handle);
            };
            m_RemoveComponentCommands.emplace_back(std::move(command));
        }

        template<typename T>
        void DispatchOnComponentAdded(World& world, EntityHandle entity_handle)
        {
            DispatchOnComponentAdded(typeid(T), world, entity_handle);
        }

        template<typename T>
        void DispatchOnComponentRemoved(World& world, EntityHandle entity_handle)
        {
            DispatchOnComponentRemoved(typeid(T), world, entity_handle);
        }

        HookHandle SubscribeOnComponentAdded(std::type_index component_type, ComponentAddedFn fn);
        HookHandle SubscribeOnComponentRemoved(std::type_index component_type, ComponentRemovedFn fn);

        void DispatchOnComponentAdded(std::type_index component_type, World& world, EntityHandle entity_handle);
        void DispatchOnComponentRemoved(std::type_index component_type, World& world, EntityHandle entity_handle);

        static uint64_t HandleToKey(EntityHandle entity_handle);

        void Flush(World& world, Ref<Scene> scene);

        using HookPoolType = HandlePool<ComponentHookBinding, HookTag>;

        HookPoolType m_HookPool;
        std::unordered_map<uint64_t, HookHandle> m_HookHandleByKey;

		std::vector<CreateEntityCommand> m_CreateCommands;
		std::vector<DestroyEntityCommand> m_DestroyCommands;
		std::vector<AddComponentCommand>    m_AddComponentCommands;
		std::vector<RemoveComponentCommand> m_RemoveComponentCommands;
		std::vector<CustomCommand> m_CustomCommands;

        DeferredEntityHandle m_NextDeferredEntity = 1;
    };
}