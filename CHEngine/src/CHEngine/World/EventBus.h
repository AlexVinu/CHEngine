#pragma once

#include <Core.h>
#include "CheStl/MemoryTypes.h"

#include <array>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ISystem.h"

namespace CHEngine {

class EventBus {
public:
    static constexpr size_t PhaseCount = static_cast<size_t>(SystemPhase::SIZE);

    template <typename Event, typename... Args>
    void Publish(SystemPhase target_phase, Args&&... args)
    {
        CHE_ASSERT(!std::is_reference_v<Event>, "Event type must not be a reference");
        auto& queue = GetOrCreateQueue<Event>(target_phase);
        queue.Publish(std::forward<Args>(args)...);
    }

    template <typename Event, typename Fn>
    void ConsumePhase(SystemPhase phase, Fn&& fn) const
    {
        CHE_ASSERT(!std::is_reference_v<Event>, "Event type must not be a reference");
        const auto& phaseQueues = m_PhaseQueues[ToIndex(phase)];
        const auto it = phaseQueues.find(std::type_index(typeid(Event)));
        if (it == phaseQueues.end())
            return;

        const auto* queue = static_cast<const EventQueue<Event>*>(it->second.get());
        queue->Consume(std::forward<Fn>(fn));
    }

    void SwapPhase(SystemPhase phase)
    {
        auto& queues = m_PhaseQueues[ToIndex(phase)];
        for (auto& [_, queue] : queues)
            queue->SwapBuffers();
    }

    void ClearPhase(SystemPhase phase)
    {
        auto& queues = m_PhaseQueues[ToIndex(phase)];
        for (auto& [_, queue] : queues)
            queue->Clear();
    }

    void ClearAll()
    {
        for (auto& phaseQueues : m_PhaseQueues)
        {
            for (auto& [_, queue] : phaseQueues)
                queue->Clear();
        }
    }

private:
    // Need for type erasure
    class IEventQueue {
    public:
        virtual ~IEventQueue() = default;
        virtual void Clear() = 0;
        virtual void SwapBuffers() = 0;
        virtual void ClearCurrent() = 0;
        virtual void ClearNext() = 0;
    };

    template <typename Event>
    class EventQueue final : public IEventQueue
    {
    public:
        template <typename... Args>
        void Publish(Args&&... args)
        {
            m_Next.emplace_back(std::forward<Args>(args)...);
        }

        template <typename Fn>
        void Consume(Fn&& fn) const
        {
            for (const Event& event : m_Current)
                fn(event);
        }

        void SwapBuffers() override
        {
            m_Current.swap(m_Next);
            m_Next.clear();
        }

        void Clear() override
        {
            m_Current.clear();
            m_Next.clear();
        }

        void ClearCurrent() override
        {
            m_Current.clear();
        }

        void ClearNext() override
        {
            m_Next.clear();
        }

    private:
        std::vector<Event> m_Current;
        std::vector<Event> m_Next;
    };

    template <typename T>
    EventQueue<T>& GetOrCreateQueue(SystemPhase phase)
    {
        const std::type_index typeId(typeid(T));
        auto& queues = m_PhaseQueues[ToIndex(phase)];
        auto it = queues.find(typeId);
        if (it == queues.end())
        {
            auto queue = std::make_unique<EventQueue<T>>();
            auto* rawQueue = queue.get();
            queues.emplace(typeId, std::move(queue));
            return *rawQueue;
        }

        return *static_cast<EventQueue<T>*>(it->second.get());
    }

    static size_t ToIndex(SystemPhase phase)
    {
        return static_cast<size_t>(phase);
    }

private:
    std::array<std::unordered_map<std::type_index, Scope<IEventQueue>>, PhaseCount> m_PhaseQueues;
};

} // namespace CHEngine
