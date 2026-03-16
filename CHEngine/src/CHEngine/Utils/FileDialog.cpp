#include "chepch.h"
#include "FileDialog.h"
#include <tinyfiledialogs.h>

namespace CHEngine {

	// ── Универсальный OpenFile с массивом фильтров ────────────────────────────
	std::string FileDialog::OpenFile(const char* filterName,
	                                 const char* const* filterPatterns,
	                                 int numFilters,
	                                 const char* title)
	{
		const char* result = tinyfd_openFileDialog(
			title ? title : "Open File",
			"",
			numFilters,
			filterPatterns,
			filterName,
			0
		);
		return result ? std::string(result) : std::string{};
	}

	// ── Шорткат: один паттерн ─────────────────────────────────────────────────
	std::string FileDialog::OpenFile(const char* filterName, const char* filterSpec)
	{
		const char* filters[] = { filterSpec };
		return OpenFile(filterName, filters, 1, filterName);
	}

	// ── Специализация для 3D-моделей ──────────────────────────────────────────
	std::string FileDialog::OpenModelFile()
	{
		const char* filters[] = { "*.obj", "*.glb", "*.gltf" };
		return OpenFile("3D Models (*.obj *.glb *.gltf)", filters, 3, "Import 3D Model");
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

		// Дописать расширение если его нет
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

	std::string FileDialog::SelectFolder(const char* title, const char* defaultPath)
	{
		const char* result = tinyfd_selectFolderDialog(
			title ? title : "Select Folder",
			defaultPath ? defaultPath : ""
		);
		return result ? std::string(result) : std::string{};
	}

}
