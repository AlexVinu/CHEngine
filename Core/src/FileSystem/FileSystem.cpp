#include "chepch.h"
#include "FileSystem.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

namespace CHEngine {

std::unique_ptr<PakReader> FileSystem::s_Pak;

// ─────────────────────────────────────────────────────────────────────────────
// Pak helpers
// ─────────────────────────────────────────────────────────────────────────────

bool FileSystem::MountPak(const std::filesystem::path& pakPath)
{
    s_Pak = std::make_unique<PakReader>();
    if (!s_Pak->Open(pakPath)) {
        s_Pak.reset();
        return false;
    }
    return true;
}

void FileSystem::UnmountPak()
{
    s_Pak.reset();
}

bool FileSystem::IsPakMounted()
{
    return s_Pak && s_Pak->IsOpen();
}

// Convert an absolute path to a pak-relative key.
// Strategy: take the path relative to the executable directory.
// Fallback: use filename only.
std::string FileSystem::ToPakKey(const std::filesystem::path& filepath)
{
    std::error_code ec;
    // Try to make relative to current_path (which Application sets to exe dir)
    auto rel = std::filesystem::relative(filepath, std::filesystem::current_path(), ec);
    if (!ec && !rel.empty() && rel.native()[0] != '.')
        return rel.generic_string();
    return filepath.filename().generic_string();
}

// ─────────────────────────────────────────────────────────────────────────────

	Buffer FileSystem::ReadFileBinary(const std::filesystem::path& filepath)
	{
		// Check pak first
		if (s_Pak && s_Pak->IsOpen()) {
			auto key   = ToPakKey(filepath);
			auto bytes = s_Pak->ReadBinary(key);
			if (!bytes.empty()) {
				Buffer buf(bytes.size());
				std::memcpy(buf.Data, bytes.data(), bytes.size());
				return buf;
			}
		}

		std::ifstream stream(filepath, std::ios::binary | std::ios::ate);
		if (!stream) return {};

		std::streampos end = stream.tellg();
		stream.seekg(0, std::ios::beg);
		const auto span = end - stream.tellg();
		if (span <= 0) return {};

		const uint64_t size = static_cast<uint64_t>(span);
		Buffer buffer(size);
		stream.read(buffer.As<char>(), static_cast<std::streamsize>(size));
		return buffer;
	}

	std::string FileSystem::ReadFileText(const std::filesystem::path& filepath)
	{
		// Check pak first
		if (s_Pak && s_Pak->IsOpen()) {
			auto key  = ToPakKey(filepath);
			auto text = s_Pak->ReadText(key);
			if (!text.empty()) return text;
		}

		std::ifstream stream(filepath, std::ios::binary | std::ios::ate);
		if (!stream) return {};

		const std::streampos end = stream.tellg();
		stream.seekg(0, std::ios::beg);
		const auto span = end - stream.tellg();
		if (span <= 0) return {};

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
		if (s_Pak && s_Pak->IsOpen() && s_Pak->Has(ToPakKey(filepath)))
			return true;
		std::error_code ec;
		return std::filesystem::exists(filepath, ec);
	}

}
