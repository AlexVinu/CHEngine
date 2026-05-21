#pragma once

#include <Core.h>

#include "CheStl/MemoryTypes.h"

#include <string>
#include <format>

namespace CHEngine {

	struct LoggerImpl;

	struct CHE_CORE_API Logger
	{
		Logger();
		~Logger();

		static void CoreLog  (int level, std::string msg);
		static void ModuleLog(int level, std::string msg);
		static void AppLog   (int level, std::string msg);

	private:
		Scope<LoggerImpl> m_Impl;
	};

} // namespace CHEngine

// Core log macros
#define CHE_CORE_TRACE(...)    ::CHEngine::Logger::CoreLog(0, std::format(__VA_ARGS__))
#define CHE_CORE_INFO(...)     ::CHEngine::Logger::CoreLog(2, std::format(__VA_ARGS__))
#define CHE_CORE_WARN(...)     ::CHEngine::Logger::CoreLog(3, std::format(__VA_ARGS__))
#define CHE_CORE_ERROR(...)    ::CHEngine::Logger::CoreLog(4, std::format(__VA_ARGS__))
#define CHE_CORE_CRITICAL(...) ::CHEngine::Logger::CoreLog(5, std::format(__VA_ARGS__))

// Module log macros
#define CHE_MODULE_TRACE(...)    ::CHEngine::Logger::ModuleLog(0, std::format(__VA_ARGS__))
#define CHE_MODULE_INFO(...)     ::CHEngine::Logger::ModuleLog(2, std::format(__VA_ARGS__))
#define CHE_MODULE_WARN(...)     ::CHEngine::Logger::ModuleLog(3, std::format(__VA_ARGS__))
#define CHE_MODULE_ERROR(...)    ::CHEngine::Logger::ModuleLog(4, std::format(__VA_ARGS__))
#define CHE_MODULE_CRITICAL(...) ::CHEngine::Logger::ModuleLog(5, std::format(__VA_ARGS__))

// Client log macros
#define CHE_TRACE(...)         ::CHEngine::Logger::AppLog(0, std::format(__VA_ARGS__))
#define CHE_INFO(...)          ::CHEngine::Logger::AppLog(2, std::format(__VA_ARGS__))
#define CHE_WARN(...)          ::CHEngine::Logger::AppLog(3, std::format(__VA_ARGS__))
#define CHE_ERROR(...)         ::CHEngine::Logger::AppLog(4, std::format(__VA_ARGS__))
#define CHE_CRITICAL(...)      ::CHEngine::Logger::AppLog(5, std::format(__VA_ARGS__))

#ifdef CHE_ENABLE_ASSERTS
	#ifdef _MSC_VER
		#define CHE_DEBUGBREAK() __debugbreak()
	#else
		#define CHE_DEBUGBREAK() __builtin_trap()
	#endif
	#define CHE_ASSERT(x, ...)		{if(!(x)) {CHE_ERROR("Assertion Failed: {0}", __VA_ARGS__); CHE_DEBUGBREAK(); } }
	#define CHE_CORE_ASSERT(x, ...) {if(!(x)) {CHE_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__); CHE_DEBUGBREAK(); } }
	#define CHE_FATAL(...) {CHE_ERROR("Fatal: {0}", __VA_ARGS__); CHE_DEBUGBREAK(); }
#else
	#define CHE_ASSERT(x, ...)
	#define CHE_CORE_ASSERT(x, ...)
	#define CHE_FATAL(...) {CHE_ERROR("Fatal: {0}", __VA_ARGS__); std::abort(); }
#endif
