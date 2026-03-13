#pragma once

#include <Core.h>
#include <string>

namespace CHEngine {

	class CHENGINE_API FileDialog
	{
	public:
		// Opens a native file dialog. Returns empty string if cancelled.
		static std::string OpenFile(const char* filterName, const char* filterSpec);
	};

}
