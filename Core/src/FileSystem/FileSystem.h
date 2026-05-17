#pragma once

#include "Core.h"
#include "Memory/Buffer.h"
// PakReader/PakBuilder moved to CHEngine/ResourceManager/AssetPack.h
// Kept here for FileSystem internal use only
#include "PakFile.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <memory>

namespace CHEngine {

	class CHE_CORE_API FileSystem
	{
	public:
		static Buffer       ReadFileBinary(const std::filesystem::path& filepath);
		static std::string  ReadFileText  (const std::filesystem::path& filepath);
		static bool WriteFileText  (const std::filesystem::path& filepath, std::string_view content);
		static bool WriteFileBinary(const std::filesystem::path& filepath, const void* data, size_t size);
		static bool Exists(const std::filesystem::path& filepath);

		// ── Pak virtual filesystem ────────────────────────────────────────────
		// Mount a .chepak file. Reads will check the pak first, then fall back
		// to the real filesystem. Only one pak can be mounted at a time.
		static bool MountPak  (const std::filesystem::path& pakPath);
		static void UnmountPak();
		static bool IsPakMounted();

	private:
		// Converts an absolute/relative filesystem path to a pak-relative key
		// by stripping the executable directory prefix if present.
		static std::string ToPakKey(const std::filesystem::path& filepath);

		static std::unique_ptr<PakReader> s_Pak;
	};

}
