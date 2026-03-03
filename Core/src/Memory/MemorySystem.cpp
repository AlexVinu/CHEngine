#include "MemorySystem.h"
#include "MallocAllocator.h"

namespace CHEngine
{
    // —татический указатель на глобальный allocator
    IAllocator* MemorySystem::s_Allocator = nullptr;

    void MemorySystem::Initialize()
    {
        // »нициализируем стандартный allocator (malloc-based)
        static MallocAllocator defaultAllocator;
        s_Allocator = &defaultAllocator;
    }

    void MemorySystem::Shutdown()
    {
        s_Allocator = nullptr;
        // ≈сли используешь динамически созданные аллокаторы Ч delete здесь
    }

    IAllocator* MemorySystem::GetAllocator()
    {
        CHE_ASSERT(s_Allocator, "MemorySystem not initialized!");
        return s_Allocator;
    }

    void MemorySystem::SetAllocator(IAllocator* allocator)
    {
        s_Allocator = allocator;
    }
}