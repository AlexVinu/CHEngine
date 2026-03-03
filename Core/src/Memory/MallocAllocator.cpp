#include"MallocAllocator.h"


namespace CHEngine
{
    void* MallocAllocator::Allocate(size_t size, size_t alignment)
    {
        #if defined(_MSC_VER)
            return _aligned_malloc(size, alignment);
        #else
            void* ptr = nullptr;
            posix_memalign(&ptr, alignment, size);
            return ptr;
        #endif
    }

    void MallocAllocator::Free(void* ptr)
    {
        #if defined(_MSC_VER)
            _aligned_free(ptr);
        #else
            free(ptr);
        #endif
    }
}