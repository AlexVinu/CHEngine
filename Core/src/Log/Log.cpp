#include "EngineContext.h"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/fmt/ostr.h>

namespace CHEngine {

    void Log::init()
    {
        spdlog::set_pattern("%^[%T] %n: %v%$");
        GetEngineContext().CoreLogger = spdlog::stdout_color_mt("CHENGINE");
        GetEngineContext().CoreLogger->set_level(spdlog::level::trace);

        GetEngineContext().ModuleLogger = spdlog::stdout_color_mt("MODULE");
        GetEngineContext().ModuleLogger->set_level(spdlog::level::trace);

        GetEngineContext().ClientLogger = spdlog::stdout_color_mt("APP");
        GetEngineContext().ClientLogger->set_level(spdlog::level::trace);
    }

    std::shared_ptr<spdlog::logger>& Log::GetCoreLogger() { return GetEngineContext().CoreLogger; }
    std::shared_ptr<spdlog::logger>& Log::GetModuleLogger() { return GetEngineContext().ModuleLogger; }
    std::shared_ptr<spdlog::logger>& Log::GetClientLogger() { return GetEngineContext().ClientLogger; }

} // namespace CHEngine