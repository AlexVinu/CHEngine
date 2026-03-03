#pragma once
#include <cstddef>

namespace CHEngine
{
    class IAllocator
    {
    public:
        virtual ~IAllocator() = default;

        virtual void* Allocate(
            size_t size,
            size_t alignment = alignof(std::max_align_t)
        ) = 0;

        virtual void Free(void* ptr) = 0;
    };
}