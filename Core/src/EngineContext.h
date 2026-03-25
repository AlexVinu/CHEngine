#pragma once

#include "Core.h"
#include "Memory/IAllocator.h"
#include "PlatformAPICapabilities.h"
#include "Log/Log.h"

namespace CHEngine
{
    struct EngineContext
    {
        IAllocator* Allocator = nullptr;

        std::shared_ptr<spdlog::logger> CoreLogger;
        std::shared_ptr<spdlog::logger> ModuleLogger;
        std::shared_ptr<spdlog::logger> ClientLogger;

        RenderAPIStore* RenderApiStore = nullptr;
    };

    CHE_CORE_API EngineContext& GetEngineContext();
}
