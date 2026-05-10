#include "chepch.h"
#include "FileSystem.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

namespace CHEngine {

	Buffer FileSystem::ReadFileBinary(const std::filesystem::path& filepath)
	{
		std::ifstream stream(filepath, std::ios::binary | std::ios::ate);

		if (!stream)
			return {};

		std::streampos end = stream.tellg();
		stream.seekg(0, std::ios::beg);
		const auto span = end - stream.tellg();
		if (span <= 0)
			return {};

		const uint64_t size = static_cast<uint64_t>(span);
		Buffer buffer(size);
		stream.read(buffer.As<char>(), static_cast<std::streamsize>(size));
		return buffer;
	}

	std::string FileSystem::ReadFileText(const std::filesystem::path& filepath)
	{
		std::ifstream stream(filepath, std::ios::binary | std::ios::ate);
		if (!stream)
			return {};

		const std::streampos end = stream.tellg();
		stream.seekg(0, std::ios::beg);
		const auto span = end - stream.tellg();
		if (span <= 0)
			return {};

		std::string out;
		out.resize(static_cast<size_t>(span));
		stream.read(out.data(), static_cast<std::streamsize>(span));
		return out;
	}

	bool FileSystem::WriteFileText(const std::filesystem::path& filepath, std::string_view content)
	{
		std::ofstream stream(filepath, std::ios::binary | std::ios::trunc);
		if (!stream)
			return false;
		stream.write(content.data(), static_cast<std::streamsize>(content.size()));
		return static_cast<bool>(stream);
	}

	bool FileSystem::WriteFileBinary(const std::filesystem::path& filepath, const void* data, size_t size)
	{
		std::ofstream stream(filepath, std::ios::binary | std::ios::trunc);
		if (!stream)
			return false;
		stream.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
		return static_cast<bool>(stream);
	}

	bool FileSystem::Exists(const std::filesystem::path& filepath)
	{
		std::error_code ec;
		return std::filesystem::exists(filepath, ec);
	}

}
