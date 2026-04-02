#pragma once

#include <Core.h>

#include "CheStl/MemoryTypes.h"

#include <memory>
#include "spdlog/spdlog.h"

namespace CHEngine {

    struct CHE_CORE_API Log
    {
        static void init();

        static Ref<spdlog::logger>& GetCoreLogger();
        static Ref<spdlog::logger>& GetModuleLogger();
        static Ref<spdlog::logger>& GetClientLogger();
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

#ifdef CHE_ENABLE_ASSERTS
	#ifdef _MSC_VER
		#define CHE_DEBUGBREAK() __debugbreak()
	#else
		#define CHE_DEBUGBREAK() __builtin_trap()
	#endif
	#define CHE_ASSERT(x, ...) {if(!(x)) {CHE_ERROR("Assertion Failed: {0}", __VA_ARGS__); CHE_DEBUGBREAK(); } }
	#define CHE_CORE_ASSERT(x, ...) {if(!(x)) {CHE_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__); CHE_DEBUGBREAK(); } }
#else
	#define CHE_ASSERT(x, ...)
	#define CHE_CORE_ASSERT(x, ...)
#endif