#pragma once
#include "Log/Log.h"

#ifdef CHE_PLATFORM_WINDOWS
	#ifdef CHE_BUILD_DLL
		#define CHENGINE_API __declspec(dllexport)
	#else
		#define CHENGINE_API __declspec(dllimport)
	#endif // CHE_

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
#else
	#error CHEngine only supports windows
#endif // CHE_PLATFORM_WINDOWS

#ifdef CHE_ENABLE_ASSERTS
	#define CHE_ASSERT(x, ...) {if(!(x)) {CHE_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
	#define CHE_CORE_ASSERT(x, ...) {if(!(x)) {CHE_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
#else
	#define CHE_ASSERT(x, ...)
	#define CHE_CORE_ASSERT(x, ...)
#endif

#define BIT(x) (1 << x)