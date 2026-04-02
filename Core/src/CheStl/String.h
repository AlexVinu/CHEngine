#pragma once

#include <string>

#include "Memory/EngineAllocator.h"

namespace CHEngine
{
	using String = std::basic_string<char, std::char_traits<char>, EngineAllocator<char>>;
}