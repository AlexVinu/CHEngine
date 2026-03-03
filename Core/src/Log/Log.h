#pragma once

#include <memory>
#include "spdlog/spdlog.h"

#ifdef CHE_LOG_EXPORT
	#ifdef _MSC_VER
		#define CHE_LOG_API __declspec(dllexport)
	#else
		#define CHE_LOG_API __attribute__((visibility("default")))
	#endif
#else
	#ifdef _MSC_VER
		#define CHE_LOG_API __declspec(dllimport)
	#else
		#define CHE_LOG_API
	#endif
#endif

namespace CHEngine {

    class CHE_LOG_API Log
    {
    public:
        static void init();

        static std::shared_ptr<spdlog::logger>& GetCoreLogger();
        static std::shared_ptr<spdlog::logger>& GetModuleLogger();
        static std::shared_ptr<spdlog::logger>& GetClientLogger();
    };

} // namespace CHEngine

// Core log macros
#define CHE_CORE_ERROR(...)    ::CHEngine::Log::GetCoreLogger()->error(__VA_ARGS__)
#define CHE_CORE_WARN(...)     ::CHEngine::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define CHE_CORE_INFO(...)     ::CHEngine::Log::GetCoreLogger()->info(__VA_ARGS__)
#define CHE_CORE_TRACE(...)    ::CHEngine::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define CHE_CORE_CRITICAL(...)    ::CHEngine::Log::GetCoreLogger()->critical(__VA_ARGS__)
													
// Module log macros
#define CHE_MODULE_ERROR(...)    ::CHEngine::Log::GetModuleLogger()->error(__VA_ARGS__)
#define CHE_MODULE_WARN(...)     ::CHEngine::Log::GetModuleLogger()->warn(__VA_ARGS__)
#define CHE_MODULE_INFO(...)     ::CHEngine::Log::GetModuleLogger()->info(__VA_ARGS__)
#define CHE_MODULE_TRACE(...)    ::CHEngine::Log::GetModuleLogger()->trace(__VA_ARGS__)
#define CHE_MODULE_CRITICAL(...)    ::CHEngine::Log::GetModuleLogger()->critical(__VA_ARGS__)

// Client log macros
#define CHE_ERROR(...)         ::CHEngine::Log::GetClientLogger()->error(__VA_ARGS__)
#define CHE_WARN(...)          ::CHEngine::Log::GetClientLogger()->warn(__VA_ARGS__)
#define CHE_INFO(...)          ::CHEngine::Log::GetClientLogger()->info(__VA_ARGS__)
#define CHE_TRACE(...)         ::CHEngine::Log::GetClientLogger()->trace(__VA_ARGS__)
#define CHE_CRITICAL(...)         ::CHEngine::Log::GetClientLogger()->critical(__VA_ARGS__)