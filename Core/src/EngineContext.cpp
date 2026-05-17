#include "EngineContext.h"

#include "Memory/MallocAllocator.h"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/fmt/ostr.h>

namespace CHEngine
{
    EngineContext::EngineContext()
    {
        // ── Logging (was Log::init()) ─────────────────────────────────────────
        // We write directly to *this* to avoid a recursive GetEngineContext() call
        // during the Meyers-singleton initialisation sequence.
        spdlog::set_pattern("%^[%T] %n: %v%$");
        CoreLogger   = spdlog::stdout_color_mt("CHENGINE");
        CoreLogger->set_level(spdlog::level::trace);
        ModuleLogger = spdlog::stdout_color_mt("MODULE");
        ModuleLogger->set_level(spdlog::level::trace);
        ClientLogger = spdlog::stdout_color_mt("APP");
        ClientLogger->set_level(spdlog::level::trace);

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
