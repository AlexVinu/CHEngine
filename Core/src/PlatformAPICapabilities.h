#pragma once

#include "RenderData.h"

namespace CHEngine {
	using RenderAPIStore = char;

	struct CHE_CORE_API RenderAPICaps
	{
		static void Initialize();
		static bool IsAvailable(ERenderAPI);
		static bool HasAnyAPI();
	};
}