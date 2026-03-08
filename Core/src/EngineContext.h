#pragma once

#include "Memory/IAllocator.h"
#include "Log/Log.h"

namespace CHEngine
{
    inline struct
    {
        IAllocator* Allocator;
        std::shared_ptr<spdlog::logger> CoreLogger;
        std::shared_ptr<spdlog::logger> ModuleLogger;
        std::shared_ptr<spdlog::logger> ClientLogger;
    } g_EngineContext;

}