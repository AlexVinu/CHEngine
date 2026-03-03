#pragma once

#include "IAllocator.h"
#include <new>
#include <utility>
#include <cstddef>

#include <Core.h>

namespace CHEngine
{
    // ============================================================
    // Raw allocation
    // ============================================================

    inline void* AllocateRaw(IAllocator* allocator, size_t size, size_t alignment)
    {
        CHE_CORE_ASSERT(allocator, "Allocator is null!");
        return allocator->Allocate(size, alignment);
    }

    inline void FreeRaw(IAllocator* allocator, void* ptr)
    {
        if (!ptr) return;
        CHE_CORE_ASSERT(allocator, "Allocator is null!");
        allocator->Free(ptr);
    }

    // ============================================================
    // Single object
    // ============================================================

    template<typename T, typename... Args>
    T* New(IAllocator* allocator, Args&&... args)
    {
        void* memory = AllocateRaw(allocator, sizeof(T), alignof(T));
        return new (memory) T(std::forward<Args>(args)...);
    }

    template<typename T>
    void Delete(IAllocator* allocator, T* object)
    {
        if (!object) return;

        object->~T();
        FreeRaw(allocator, object);
    }

    // ============================================================
    // Array allocation
    // ============================================================

    template<typename T>
    T* NewArray(IAllocator* allocator, size_t count)
    {
        size_t totalSize = sizeof(T) * count;

        void* memory = AllocateRaw(allocator, totalSize, alignof(T));
        T* array = static_cast<T*>(memory);

        for (size_t i = 0; i < count; ++i)
            new (&array[i]) T();

        return array;
    }

    template<typename T>
    void DeleteArray(IAllocator* allocator, T* array, size_t count)
    {
        if (!array) return;

        for (size_t i = 0; i < count; ++i)
            array[i].~T();

        FreeRaw(allocator, array);
    }

}