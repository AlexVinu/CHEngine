#include "Log.h"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/fmt/ostr.h>

namespace CHEngine {

    static std::shared_ptr<spdlog::logger> s_CoreLogger;
    static std::shared_ptr<spdlog::logger> s_ModuleLogger;
    static std::shared_ptr<spdlog::logger> s_ClientLogger;

    void Log::init()
    {
        spdlog::set_pattern("%^[%T] %n: %v%$");
        s_CoreLogger = spdlog::stdout_color_mt("CHENGINE");
        s_CoreLogger->set_level(spdlog::level::trace);

        s_ModuleLogger = spdlog::stdout_color_mt("MODULE");
        s_ModuleLogger->set_level(spdlog::level::trace);

        s_ClientLogger = spdlog::stdout_color_mt("APP");
        s_ClientLogger->set_level(spdlog::level::trace);
    }

    std::shared_ptr<spdlog::logger>& Log::GetCoreLogger() { return s_CoreLogger; }
    std::shared_ptr<spdlog::logger>& Log::GetModuleLogger() { return s_ModuleLogger; }
    std::shared_ptr<spdlog::logger>& Log::GetClientLogger() { return s_ClientLogger; }

} // namespace CHEngine