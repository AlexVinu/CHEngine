#pragma once

#include <string>

#include "Memory/EngineAllocator.h"

using String = std::basic_string<char, std::char_traits<char>, CHEngine::EngineAllocator<char>>;
