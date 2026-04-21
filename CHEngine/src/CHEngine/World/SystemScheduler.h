#pragma once
#include <Core.h>
#include "CheStl/MemoryTypes.h"
#include <algorithm>
#include <memory>
#include <array>
#include <string_view>
#include <vector>

#include "ISystem.h"

namespace CHEngine
{
    class World;

    class SystemScheduler {
    public:
        static constexpr size_t PhaseCount = (size_t)SystemPhase::SIZE;

        template<typename T, typename... Args>
        T& EmplaceSystem(Args&&... args) {
            auto system = std::make_unique<T>(std::forward<Args>(args)...);
            T& ref = *system;
            AddSystem(std::move(system));
            return ref;
        }

        ISystem& AddSystem(Scope<ISystem> system);

        bool SetEnabled(const ISystem& system, bool enabled);
        bool SetEnabled(std::string_view system_name, bool enabled);
        bool IsEnabled(const ISystem& system) const;
        bool IsEnabled(std::string_view system_name) const;

        void RunPhase(SystemPhase phase, World& world, DeferredOps& deferred_ops, Timestep dt);

        void NotifyBegin(SystemPhase phase, World& world, DeferredOps& deferred_ops);
        void NotifyEnd(SystemPhase phase, World& world, DeferredOps& deferred_ops);

    private:
        void SortPhase(SystemPhase phase);
        std::vector<Scope<ISystem>>& GetPhaseList(SystemPhase phase);
        const std::vector<Scope<ISystem>>& GetPhaseList(SystemPhase phase) const;

        std::array<std::vector<Scope<ISystem>>, PhaseCount> m_Phases;
    };
}
