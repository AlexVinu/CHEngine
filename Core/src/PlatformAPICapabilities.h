#pragma once

#include "RenderData.h"

namespace CHEngine {
	struct CHE_CORE_API RenderModuleResolver
	{
		static bool TryParseRenderAPI(const char* value, ERenderAPI* out_api);
		static const char* ToConfigName(ERenderAPI api);
		static ModuleNames GetModuleNames(ERenderAPI api);
		static bool IsSupportedOnPlatform(ERenderAPI api);
	};
}
