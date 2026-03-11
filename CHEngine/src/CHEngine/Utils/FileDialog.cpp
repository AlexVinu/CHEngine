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

}
