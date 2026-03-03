#pragma once
#include "Log/Log.h"

#ifdef CHE_PLATFORM_WINDOWS
	#ifdef CHE_BUILD_DLL
		#define CHENGINE_API __declspec(dllexport)
	#else
		#define CHENGINE_API __declspec(dllimport)
	#endif

	#ifdef CHE_BUILD_MODULE_DLL
		#define CHE_MODULE_API __declspec(dllexport)
	#else
		#define CHE_MODULE_API __declspec(dllimport)
	#endif

	#ifdef CHE_BUILD_CORE_DLL
		#define CHE_CORE_API __declspec(dllexport)
	#else
		#define CHE_CORE_API __declspec(dllimport)
	#endif
#elif defined(CHE_PLATFORM_LINUX) || defined(CHE_PLATFORM_APPLE)
	#ifdef CHE_BUILD_DLL
		#define CHENGINE_API __attribute__((visibility("default")))
	#else
		#define CHENGINE_API
	#endif

	#ifdef CHE_BUILD_MODULE_DLL
		#define CHE_MODULE_API __attribute__((visibility("default")))
	#else
		#define CHE_MODULE_API
	#endif

	#ifdef CHE_BUILD_CORE_DLL
		#define CHE_CORE_API __attribute__((visibility("default")))
	#else
		#define CHE_CORE_API
	#endif
#else
	#error Unsupported platform
#endif

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

#define BIT(x) (1 << x)
