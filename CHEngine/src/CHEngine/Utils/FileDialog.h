#pragma once

#include <Core.h>
#include <string>

namespace CHEngine {

	class CHENGINE_API FileDialog
	{
	public:
		// Универсальный диалог открытия файла.
		// filterPatterns: массив паттернов, например {"*.chscene"}
		// filterName: строка описания, например "Scene Files"
		static std::string OpenFile(const char* filterName,
		                            const char* const* filterPatterns,
		                            int numFilters,
		                            const char* title = "Open File");

		// Шорткат для одного расширения: OpenFile("*.obj", "3D Models")
		static std::string OpenFile(const char* filterName, const char* filterSpec);

		// Диалог импорта 3D-моделей (OBJ / GLB / GLTF)
		static std::string OpenModelFile();

		// Opens a native folder-select dialog. Returns chosen path or empty string if cancelled.
		static std::string SelectFolder(const char* title, const char* defaultPath = nullptr);

		// Opens a native save-file dialog. Returns chosen path or empty string if cancelled.
		// filterPatterns: array of patterns like "*.chscene". defaultExt appended if missing.
		static std::string SaveFile(const char* title,
		                            const char* defaultPath,
		                            const char* const* filterPatterns,
		                            int numFilters,
		                            const char* defaultExt = nullptr);
	};

}
