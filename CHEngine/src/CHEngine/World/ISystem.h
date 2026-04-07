#pragma once

#include <cstdint>

#include "CHEngine/Events/Event.h"
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
    class CommandBuffer;

    class ISystem {
    public:
        ISystem(SystemPhase phase, uint8_t priority, bool enabled = true)
            : m_Phase(phase), m_Priority(priority), m_Enabled(enabled) {}
        virtual ~ISystem() = default;

        SystemPhase phase() const { return m_Phase; }
        virtual uint8_t priority() const { return m_Priority; }
        virtual const char* name() const = 0;
        virtual void run(World& world, CommandBuffer& commands, Timestep dt) = 0;
        virtual void onEvent(Event&, World&, CommandBuffer&) {}

        bool enabled() const { return m_Enabled; }
        void setEnabled(bool value) { m_Enabled = value; }

    private:
        uint8_t m_Priority;
        SystemPhase m_Phase;
        bool m_Enabled = true;
    };
}