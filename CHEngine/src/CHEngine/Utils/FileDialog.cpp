#include "chepch.h"
#include "FileDialog.h"
#include <tinyfiledialogs.h>

namespace CHEngine {

	std::string FileDialog::OpenFile(const char* filterName, const char* filterSpec)
	{
		const char* filters[] = { "*.obj", "*.glb", "*.gltf" };
		(void)filterSpec;

		const char* result = tinyfd_openFileDialog(
			"Import 3D Model",
			"",
			3,
			filters,
			filterName,
			0
		);

		if (result)
			return std::string(result);
		return {};
	}

	std::string FileDialog::SaveFile(const char* title,
	                                 const char* defaultPath,
	                                 const char* const* filterPatterns,
	                                 int numFilters,
	                                 const char* defaultExt)
	{
		const char* result = tinyfd_saveFileDialog(
			title,
			defaultPath ? defaultPath : "",
			numFilters,
			filterPatterns,
			nullptr
		);

		if (!result)
			return {};

		std::string path(result);

		// Append default extension if it's not already present
		if (defaultExt && !path.empty()) {
			std::string ext(defaultExt);
			if (path.size() < ext.size() ||
			    path.compare(path.size() - ext.size(), ext.size(), ext) != 0)
			{
				path += ext;
			}
		}

		return path;
	}

}
