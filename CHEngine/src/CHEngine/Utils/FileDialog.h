#pragma once

#include <Core.h>
#include <string>

namespace CHEngine {

	class CHENGINE_API FileDialog
	{
	public:
		// Opens a native open-file dialog. Returns empty string if cancelled.
		static std::string OpenFile(const char* filterName, const char* filterSpec);

		// Opens a native save-file dialog. Returns chosen path or empty string if cancelled.
		// filterPatterns: array of patterns like "*.chscene". defaultExt appended if missing.
		static std::string SaveFile(const char* title,
		                            const char* defaultPath,
		                            const char* const* filterPatterns,
		                            int numFilters,
		                            const char* defaultExt = nullptr);
	};

}
