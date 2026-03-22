#pragma once

#include "RenderData.h"
#include "BitwiseIterator.h"

namespace CHEngine {
	using RenderAPIStore = uint8_t;

	struct ModuleNames
	{
		const char* Renderer;
		const char* ImGui;
	};

	struct CHE_CORE_API RenderAPICaps
	{
		static void Initialize();
		static void SetFlag(ERenderAPI, bool);
		static bool IsAvailable(ERenderAPI);
		static BitwiseRange<ERenderAPI, RenderAPIStore> AllAvailableAPI();
		static bool HasAnyAPI();

		static ModuleNames GetModuleNames(ERenderAPI api);
	};
}