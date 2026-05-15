#pragma once

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

#define BIT(x) (1 << x)

#define BIND_EVENT_FN(fn) \
    [this](auto&&... args) -> decltype(auto) { \
        return this->fn(std::forward<decltype(args)>(args)...); \
    }
