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

        Ref<spdlog::logger> CoreLogger;
        Ref<spdlog::logger> ModuleLogger;
        Ref<spdlog::logger> ClientLogger;

        RenderAPIStore* RenderApiStore = nullptr;
    };

    CHE_CORE_API EngineContext& GetEngineContext();
}
