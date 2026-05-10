#pragma once

#include "MemorySystem.h"

#include <cstddef>
#include <limits>
#include <new>

namespace CHEngine
{
    template<typename T>
    class EngineAllocator
    {
    public:
        using value_type = T;

        EngineAllocator() noexcept
            : m_Allocator(MemorySystem::GetAllocator())
        {
        }

        explicit EngineAllocator(IAllocator* allocator) noexcept
            : m_Allocator(allocator)
        {
        }

        template<typename U>
        EngineAllocator(const EngineAllocator<U>& other) noexcept
            : m_Allocator(other.m_Allocator)
        {
        }

        T* allocate(std::size_t n)
        {
            if (n == 0)
                n = 1;
            if (n > std::numeric_limits<std::size_t>::max() / sizeof(T))
                throw std::bad_alloc();
            void* p = m_Allocator->Allocate(n * sizeof(T), alignof(T));
            if (!p)
                throw std::bad_alloc();
            return static_cast<T*>(p);
        }

        void deallocate(T* p, std::size_t n) noexcept
        {
            (void)n;
            if (p)
                m_Allocator->Free(p);
        }

        friend bool operator==(const EngineAllocator& a, const EngineAllocator& b) noexcept
        {
            return a.m_Allocator == b.m_Allocator;
        }

        friend bool operator!=(const EngineAllocator& a, const EngineAllocator& b) noexcept
        {
            return !(a == b);
        }

    private:
        template<typename U>
        friend class EngineAllocator;

        IAllocator* m_Allocator;
    };
}
