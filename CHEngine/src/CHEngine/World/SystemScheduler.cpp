#include "SystemScheduler.h"
#include "World.h"

namespace CHEngine {

    namespace {
        bool PriorityLess(const Scope<ISystem>& left, const Scope<ISystem>& right)
        {
            return left->GetPriority() < right->GetPriority();
        }
    } // namespace

    ISystem& SystemScheduler::AddSystem(Scope<ISystem> system)
    {
        CHE_CORE_ASSERT(system, "SystemScheduler::AddSystem expects a valid system instance");
        ISystem* raw = system.get();
        auto& list = GetPhaseList(system->GetPhase());
        list.push_back(std::move(system));
        SortPhase(raw->GetPhase());
        return *raw;
    }

    bool SystemScheduler::SetEnabled(const ISystem& system, bool enabled)
    {
        for (auto& phaseList : m_Phases) {
            for (auto& candidate : phaseList) {
                if (candidate.get() != &system)
                    continue;
                candidate->SetEnabled(enabled);
                return true;
            }
        }
        return false;
    }

    bool SystemScheduler::SetEnabled(std::string_view system_name, bool enabled)
    {
        for (auto& phaseList : m_Phases) {
            for (auto& system : phaseList) {
                if (system_name != system->GetName())
                    continue;
                system->SetEnabled(enabled);
                return true;
            }
        }
        return false;
    }

    bool SystemScheduler::IsEnabled(const ISystem& system) const
    {
        for (const auto& phaseList : m_Phases) {
            for (const auto& candidate : phaseList) {
                if (candidate.get() != &system)
                    continue;
                return candidate->IsEnabled();
            }
        }
        return false;
    }

    bool SystemScheduler::IsEnabled(std::string_view system_name) const
    {
        for (const auto& phaseList : m_Phases) {
            for (const auto& system : phaseList) {
                if (system_name != system->GetName())
                    continue;
                return system->IsEnabled();
            }
        }
        return false;
    }

    void SystemScheduler::RunPhase(SystemPhase phase, World& world, DeferredOps& deferred_ops, Timestep dt)
    {
        auto& list = GetPhaseList(phase);
        for (size_t i = 0; i < list.size(); ++i) {
            ISystem* system = list[i].get();
            if (!system || !system->IsEnabled())
                continue;
            system->Run(world, deferred_ops, dt);
        }

        world.GetEvents().SwapPhase(phase);
        for (size_t i = 0; i < list.size(); ++i) {
            ISystem* system = list[i].get();
            if (!system || !system->IsEnabled())
                continue;
            system->OnPhaseDispatch(world, deferred_ops);
        }
    }

    void SystemScheduler::SortPhase(SystemPhase phase)
    {
        auto& list = GetPhaseList(phase);
        std::stable_sort(list.begin(), list.end(), PriorityLess);
    }

    std::vector<Scope<ISystem>>& SystemScheduler::GetPhaseList(SystemPhase phase)
    {
        return m_Phases[static_cast<size_t>(phase)];
    }

    const std::vector<Scope<ISystem>>& SystemScheduler::GetPhaseList(SystemPhase phase) const
    {
        return m_Phases[static_cast<size_t>(phase)];
    }
}
