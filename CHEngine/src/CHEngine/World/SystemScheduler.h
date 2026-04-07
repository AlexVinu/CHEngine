#pragma once
#include <algorithm>
#include <array>
#include <memory>
#include <string_view>
#include <vector>

#include "CommandBuffer.h"
#include "ISystem.h"

namespace CHEngine
{
    class World;

    class SystemScheduler {
    public:
        static constexpr size_t PhaseCount = (size_t)SystemPhase::SIZE;

        template<typename T, typename... Args>
        T& emplaceSystem(Args&&... args) {
            auto system = std::make_unique<T>(std::forward<Args>(args)...);
            T& ref = *system;
            addSystem(std::move(system));
            return ref;
        }

        ISystem& addSystem(std::unique_ptr<ISystem> system);

        bool setEnabled(const ISystem& system, bool enabled);
        bool setEnabled(std::string_view systemName, bool enabled);
        bool isEnabled(const ISystem& system) const;
        bool isEnabled(std::string_view systemName) const;

        void runPhase(SystemPhase phase, World& world, CommandBuffer& commands, Timestep dt);
        void dispatchEvent(Event& event, World& world, CommandBuffer& commands);

    private:
        void sortPhase(SystemPhase phase);
        std::vector<std::unique_ptr<ISystem>>& getPhaseList(SystemPhase phase);
        const std::vector<std::unique_ptr<ISystem>>& getPhaseList(SystemPhase phase) const;

        std::array<std::vector<std::unique_ptr<ISystem>>, PhaseCount> phases_;
    };
}