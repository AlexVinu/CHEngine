#pragma once
#include "IAllocator.h"
#include <cstdlib>

namespace CHEngine
{
    class MallocAllocator : public IAllocator
    {
    public:
        void* Allocate(size_t size, size_t alignment) override
        {
            #if defined(_MSC_VER)
            return _aligned_malloc(size, alignment);
            #else
            void* ptr = nullptr;
            if (alignment < sizeof(void*))
                alignment = sizeof(void*);
            posix_memalign(&ptr, alignment, size);
            return ptr;
            #endif
        }

        void Free(void* ptr) override
        {
            #if defined(_MSC_VER)
            _aligned_free(ptr);
            #else
            free(ptr);
            #endif
        }
    };
}
