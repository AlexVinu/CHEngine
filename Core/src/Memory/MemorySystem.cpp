#include "MemorySystem.h"
#include "MallocAllocator.h"
#include "EngineContext.h"

namespace CHEngine
{
    void MemorySystem::Initialize()
    {
        static MallocAllocator defaultAllocator;
        GetEngineContext().Allocator = &defaultAllocator;
    }

    void MemorySystem::Shutdown()
    {
        GetEngineContext().Allocator = nullptr;
    }

    IAllocator* MemorySystem::GetAllocator()
    {
        CHE_ASSERT(GetEngineContext().Allocator, "MemorySystem not initialized!");
        return GetEngineContext().Allocator;
    }

    void MemorySystem::SetAllocator(IAllocator* allocator)
    {
        GetEngineContext().Allocator = allocator;
    }
}