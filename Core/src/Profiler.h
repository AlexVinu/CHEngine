#pragma once

#include "Core.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4251)
#endif

#include <chrono>
#include <string_view>
#include <unordered_map>
#include <mutex>
#include <vector>

#if !defined(CHE_PROFILING_ENABLED) && !defined(NDEBUG)
#define CHE_PROFILING_ENABLED
#endif

namespace CHEngine {

    class CHE_CORE_API Profiler
    {
    public:
        struct Stat
        {
            double totalMs = 0.0;
            int    calls   = 0;
            double maxMs   = 0.0;
        };

        struct PtrHash {
            size_t operator()(const char* p) const {
                return std::hash<std::string_view>{}(p);
            }
        };
        struct PtrEqual {
            bool operator()(const char* a, const char* b) const {
                return std::string_view(a) == std::string_view(b);
            }
        };

        using StatsMap = std::unordered_map<const char*, Stat, PtrHash, PtrEqual>;

        static void AddSample(const char* name, double ms);
        static void Flush();
        static const StatsMap& GetFrameStats();
        static void Reset();

    private:
        struct ThreadLocalData {
            StatsMap samples;
        };
        static ThreadLocalData& GetThreadLocal();
        static void RegisterThread(ThreadLocalData* tld);

        static std::mutex                    s_Mutex;
        static std::vector<ThreadLocalData*> s_AllThreads;
        static StatsMap                      s_FrameStats;
    };

    class CHE_CORE_API ScopeTimer
    {
    public:
        explicit ScopeTimer(const char* name);
        ~ScopeTimer();

    private:
        const char* name_;
        std::chrono::high_resolution_clock::time_point start_;
    };
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#define CHE_CONCAT_IMPL(a, b) a##b
#define CHE_CONCAT(a, b) CHE_CONCAT_IMPL(a, b)

#if defined(__GNUC__) || (defined(__MWERKS__) && (__MWERKS__ >= 0x3000)) || (defined(__ICC) && (__ICC >= 600)) || defined(__ghs__)
#define CHE_FUNC_SIG __PRETTY_FUNCTION__
#elif defined(__DMC__) && (__DMC__ >= 0x810)
#define CHE_FUNC_SIG __PRETTY_FUNCTION__
#elif (defined(__FUNCSIG__) || (_MSC_VER))
#define CHE_FUNC_SIG __FUNCSIG__
#elif (defined(__INTEL_COMPILER) && (__INTEL_COMPILER >= 600)) || (defined(__IBMCPP__) && (__IBMCPP__ >= 500))
#define CHE_FUNC_SIG __FUNCTION__
#elif defined(__BORLANDC__) && (__BORLANDC__ >= 0x550)
#define CHE_FUNC_SIG __FUNC__
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901)
#define CHE_FUNC_SIG __func__
#elif defined(__cplusplus) && (__cplusplus >= 201103)
#define CHE_FUNC_SIG __func__
#else
#define CHE_FUNC_SIG "CHE_FUNC_SIG unknown!"
#endif

#ifdef CHE_PROFILING_ENABLED
    #define CHE_PROFILE_SCOPE(name) ::CHEngine::ScopeTimer CHE_CONCAT(cheTimer_, __LINE__)(name)
    #define CHE_PROFILE_FUNCTION()  CHE_PROFILE_SCOPE(CHE_FUNC_SIG)
#else
    #define CHE_PROFILE_SCOPE(name)
    #define CHE_PROFILE_FUNCTION()
#endif
