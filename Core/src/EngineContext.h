#pragma once

#include "Memory/IAllocator.h"

namespace CHEngine
{
    struct EngineContext
    {
        IAllocator* Allocator;
    };
}