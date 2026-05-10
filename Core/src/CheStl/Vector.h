#pragma once

#include <vector>

#include "Memory/EngineAllocator.h"

namespace CHEngine
{
    template<typename T>
    using Vector = std::vector<T, EngineAllocator<T>>;
}

template<typename T>
using Vector = CHEngine::Vector<T>;

