#include "EngineContext.h"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/fmt/ostr.h>

namespace CHEngine {

    void Log::init()
    {
        spdlog::set_pattern("%^[%T] %n: %v%$");
        g_EngineContext.CoreLogger = spdlog::stdout_color_mt("CHENGINE");
        g_EngineContext.CoreLogger->set_level(spdlog::level::trace);

        g_EngineContext.ModuleLogger = spdlog::stdout_color_mt("MODULE");
        g_EngineContext.ModuleLogger->set_level(spdlog::level::trace);

        g_EngineContext.ClientLogger = spdlog::stdout_color_mt("APP");
        g_EngineContext.ClientLogger->set_level(spdlog::level::trace);
    }

    std::shared_ptr<spdlog::logger>& Log::GetCoreLogger() { return g_EngineContext.CoreLogger; }
    std::shared_ptr<spdlog::logger>& Log::GetModuleLogger() { return g_EngineContext.ModuleLogger; }
    std::shared_ptr<spdlog::logger>& Log::GetClientLogger() { return g_EngineContext.ClientLogger; }

} // namespace CHEngine