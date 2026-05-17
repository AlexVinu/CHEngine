#include "EngineContext.h"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/fmt/ostr.h>

namespace CHEngine {

    void Log::init()
    {
        // No-op: logging is now initialised by EngineContext::EngineContext().
        // The first call to GetEngineContext() (triggered by any CHE_CORE_* macro
        // or direct access) initialises the Meyers-singleton and its constructor
        // sets up spdlog loggers.  Calling Log::init() explicitly is no longer
        // required and is kept only for backwards compatibility.
        (void)GetEngineContext(); // ensure the singleton is alive
    }

    Ref<spdlog::logger>& Log::GetCoreLogger() { return GetEngineContext().CoreLogger; }
    Ref<spdlog::logger>& Log::GetModuleLogger() { return GetEngineContext().ModuleLogger; }
    Ref<spdlog::logger>& Log::GetClientLogger() { return GetEngineContext().ClientLogger; }

} // namespace CHEngine
