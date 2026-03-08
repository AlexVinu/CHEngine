#include "MemorySystem.h"
#include "MallocAllocator.h"

namespace CHEngine
{
    void MemorySystem::Initialize()
    {
        static MallocAllocator defaultAllocator;
        g_EngineContext.Allocator = &defaultAllocator;
    }

    void MemorySystem::Shutdown()
    {
        g_EngineContext.Allocator = nullptr;
    }

    IAllocator* MemorySystem::GetAllocator()
    {
        CHE_ASSERT(g_EngineContext.Allocator, "MemorySystem not initialized!");
        return g_EngineContext.Allocator;
    }

    void MemorySystem::SetAllocator(IAllocator* allocator)
    {
        g_EngineContext.Allocator = allocator;
    }
}