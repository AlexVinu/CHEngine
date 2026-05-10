#pragma once

#include "Core.h"
#include "Memory/Buffer.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace CHEngine {

	class CHE_CORE_API FileSystem
	{
	public:
		static Buffer ReadFileBinary(const std::filesystem::path& filepath);
		static std::string ReadFileText(const std::filesystem::path& filepath);
		static bool WriteFileText(const std::filesystem::path& filepath, std::string_view content);
		static bool WriteFileBinary(const std::filesystem::path& filepath, const void* data, size_t size);
		static bool Exists(const std::filesystem::path& filepath);
	};

}
