#include "SystemScheduler.h"
#include "World.h"

namespace CHEngine {

    namespace {
        bool PriorityLess(const std::unique_ptr<ISystem>& left, const std::unique_ptr<ISystem>& right)
        {
            return left->priority() < right->priority();
        }
    } // namespace

    ISystem& SystemScheduler::addSystem(std::unique_ptr<ISystem> system)
    {
        CHE_CORE_ASSERT(system, "SystemScheduler::addSystem expects a valid system instance");
        ISystem* raw = system.get();
        auto& list = getPhaseList(system->phase());
        list.push_back(std::move(system));
        sortPhase(raw->phase());
        return *raw;
    }

    bool SystemScheduler::setEnabled(const ISystem& system, bool enabled)
    {
        for (auto& phaseList : phases_) {
            for (auto& candidate : phaseList) {
                if (candidate.get() != &system)
                    continue;
                candidate->setEnabled(enabled);
                return true;
            }
        }
        return false;
    }

    bool SystemScheduler::setEnabled(std::string_view systemName, bool enabled)
    {
        for (auto& phaseList : phases_) {
            for (auto& system : phaseList) {
                if (systemName != system->name())
                    continue;
                system->setEnabled(enabled);
                return true;
            }
        }
        return false;
    }

    bool SystemScheduler::isEnabled(const ISystem& system) const
    {
        for (const auto& phaseList : phases_) {
            for (const auto& candidate : phaseList) {
                if (candidate.get() != &system)
                    continue;
                return candidate->enabled();
            }
        }
        return false;
    }

    bool SystemScheduler::isEnabled(std::string_view systemName) const
    {
        for (const auto& phaseList : phases_) {
            for (const auto& system : phaseList) {
                if (systemName != system->name())
                    continue;
                return system->enabled();
            }
        }
        return false;
    }

    void SystemScheduler::runPhase(SystemPhase phase, World& world, CommandBuffer& commands, Timestep dt)
    {
        auto& list = getPhaseList(phase);
        for (size_t i = 0; i < list.size(); ++i) {
            ISystem* system = list[i].get();
            if (!system || !system->enabled())
                continue;
            system->run(world, commands, dt);
        }
    }

    void SystemScheduler::dispatchEvent(Event& event, World& world, CommandBuffer& commands)
    {
        for (size_t phaseIndex = 0; phaseIndex < phases_.size(); ++phaseIndex) {
            auto& list = phases_[phaseIndex];
            for (size_t i = 0; i < list.size(); ++i) {
                ISystem* system = list[i].get();
                if (!system || !system->enabled())
                    continue;
                system->onEvent(event, world, commands);
                if (event.Handled)
                    return;
            }
        }
    }

    void SystemScheduler::sortPhase(SystemPhase phase)
    {
        auto& list = getPhaseList(phase);
        std::stable_sort(list.begin(), list.end(), PriorityLess);
    }

    std::vector<std::unique_ptr<ISystem>>& SystemScheduler::getPhaseList(SystemPhase phase)
    {
        return phases_[static_cast<size_t>(phase)];
    }

    const std::vector<std::unique_ptr<ISystem>>& SystemScheduler::getPhaseList(SystemPhase phase) const
    {
        return phases_[static_cast<size_t>(phase)];
    }
}