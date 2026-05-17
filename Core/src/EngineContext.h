#pragma once

#include "Core.h"
#include "Memory/IAllocator.h"
#include "Log/Log.h"

namespace CHEngine
{
    // Process-scoped RAII context. The Meyers-singleton in GetEngineContext()
    // ensures exactly one instance per process. The constructor initialises
    // logging and the default allocator; the destructor tears them down.
    class CHE_CORE_API EngineContext
    {
    public:
        EngineContext();
        ~EngineContext();

        EngineContext(const EngineContext&) = delete;
        EngineContext& operator=(const EngineContext&) = delete;

        // Public data — legacy callers (Log, MemorySystem) access these directly.
        IAllocator* Allocator = nullptr;

        Ref<spdlog::logger> CoreLogger;
        Ref<spdlog::logger> ModuleLogger;
        Ref<spdlog::logger> ClientLogger;
    };

    CHE_CORE_API EngineContext& GetEngineContext();
}
