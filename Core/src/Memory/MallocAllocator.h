#pragma once
#include "IAllocator.h"
#include <cstdlib>

namespace CHEngine
{
    class MallocAllocator : public IAllocator
    {
    public:
        void* Allocate(size_t size, size_t alignment) override;

        void Free(void* ptr) override;
    };
}