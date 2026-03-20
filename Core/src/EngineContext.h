#pragma once

#include "Memory/IAllocator.h"
#include "PlatformAPICapabilities.h"
#include "Log/Log.h"

namespace CHEngine
{
    inline struct
    {
        // Memory -------------------
        IAllocator* Allocator;
        // Memory ===================
        // Logging ------------------
        std::shared_ptr<spdlog::logger> CoreLogger;
        std::shared_ptr<spdlog::logger> ModuleLogger;
        std::shared_ptr<spdlog::logger> ClientLogger;
        // Logging ==================
        // System dependencies ------
        RenderAPIStore* RenderApiStore;
        // System dependencies ======
    } g_EngineContext;

}