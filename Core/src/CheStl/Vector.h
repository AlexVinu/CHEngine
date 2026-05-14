#pragma once

#include <vector>

#include "Memory/EngineAllocator.h"

template<typename T>
using Vector = std::vector<T, CHEngine::EngineAllocator<T>>;

