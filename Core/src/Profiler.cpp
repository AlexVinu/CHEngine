#include "Profiler.h"

namespace CHEngine {

    std::mutex                               Profiler::s_Mutex;
    std::vector<Profiler::ThreadLocalData*>  Profiler::s_AllThreads;
    Profiler::StatsMap                       Profiler::s_FrameStats;

    Profiler::ThreadLocalData& Profiler::GetThreadLocal()
    {
        thread_local ThreadLocalData tld;
        static thread_local bool registered = [&] {
            RegisterThread(&tld);
            return true;
        }();
        (void)registered;
        return tld;
    }

    void Profiler::RegisterThread(ThreadLocalData* tld)
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        s_AllThreads.push_back(tld);
    }

    void Profiler::AddSample(const char* name, double ms)
    {
        auto& tld = GetThreadLocal();
        auto& stat = tld.samples[name];
        stat.totalMs += ms;
        stat.calls += 1;
        if (ms > stat.maxMs) stat.maxMs = ms;
    }

    void Profiler::Flush()
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        s_FrameStats.clear();
        for (auto* tld : s_AllThreads)
        {
            for (auto& [name, stat] : tld->samples)
            {
                auto& dst = s_FrameStats[name];
                dst.totalMs += stat.totalMs;
                dst.calls   += stat.calls;
                if (stat.maxMs > dst.maxMs) dst.maxMs = stat.maxMs;
            }
            tld->samples.clear();
        }
    }

    const Profiler::StatsMap& Profiler::GetFrameStats()
    {
        return s_FrameStats;
    }

    void Profiler::Reset()
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        s_FrameStats.clear();
        for (auto* tld : s_AllThreads)
            tld->samples.clear();
    }

    ScopeTimer::ScopeTimer(const char* name)
        : name_(name),
        start_(std::chrono::high_resolution_clock::now())
    {
    }

    ScopeTimer::~ScopeTimer()
    {
        const auto end = std::chrono::high_resolution_clock::now();
        const double ms = std::chrono::duration<double, std::milli>(end - start_).count();
        Profiler::AddSample(name_, ms);
    }
}
