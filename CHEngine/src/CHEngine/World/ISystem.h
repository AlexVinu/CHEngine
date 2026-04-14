#pragma once

#include <cstdint>

#include "Timestep.h"

namespace CHEngine
{
    enum class SystemPhase : uint8_t {
        Initialization,
        Simulation,
        Presentation,
        SIZE
    };

    class World;
    class DeferredOps;

    class ISystem {
    public:
        ISystem(SystemPhase system_phase, uint8_t priority, bool enabled = true)
            : m_Priority(priority), m_Phase(system_phase), m_Enabled(enabled) {}
        virtual ~ISystem() = default;

        SystemPhase GetPhase() const { return m_Phase; }
        virtual uint8_t GetPriority() const { return m_Priority; }
        virtual const char* GetName() const = 0;
        virtual void Run(World& world, DeferredOps& deferred_ops, Timestep dt) = 0;
        virtual void OnPhaseDispatch(World& world, DeferredOps& deferred_ops)
        {
        }

        bool IsEnabled() const { return m_Enabled; }
        void SetEnabled(bool value) { m_Enabled = value; }

    private:
        uint8_t m_Priority;
        SystemPhase m_Phase;
        bool m_Enabled = true;
    };
}
