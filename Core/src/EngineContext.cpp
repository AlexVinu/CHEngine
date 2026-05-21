#include "EngineContext.h"

#include "Memory/MallocAllocator.h"

namespace CHEngine
{
    EngineContext::EngineContext()
    {
        // Logger инициализируется в Logger::Logger() через Scope<Logger> logger

        // ── Memory (was MemorySystem::Initialize()) ───────────────────────────
        static MallocAllocator s_DefaultAllocator;
        Allocator = &s_DefaultAllocator;
    }

    EngineContext::~EngineContext()
    {
        // ── Memory (was MemorySystem::Shutdown()) ─────────────────────────────
        Allocator = nullptr;
    }

    EngineContext& GetEngineContext()
    {
        static EngineContext ctx;
        return ctx;
    }
}
