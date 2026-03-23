#pragma once

#include "RenderData.h"
#include "BitwiseIterator.h"

namespace CHEngine {
	using RenderAPIStore = uint8_t;

	struct CHE_CORE_API RenderAPICaps
	{
		static void Initialize();
		static void SetFlag(WRenderAPI, bool);
		static bool IsAvailable(WRenderAPI);
		static BitwiseRange<WRenderAPI, RenderAPIStore> AllAvailableAPI();
		static bool HasAnyAPI();

		static ModuleNames GetModuleNames(WRenderAPI api);
	};
}