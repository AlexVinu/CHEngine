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

// Convert an arbitrary (usually absolute) path to a pak-relative key.
//
// Editor safety: when no pak is mounted this returns "" immediately, so the
// function is a no-op for the editor and all reads fall through to disk.
//
// When a pak IS mounted we don't rely on current_path() (fragile). Instead we
// try every trailing sub-path of `filepath`, longest first, and return the
// longest one the pak actually contains. This makes packed assets resolve
// reliably no matter what absolute prefix the caller passed
// (e.g. ".../bin/Assets/Models/x.glb" → "Assets/Models/x.glb").
std::string FileSystem::ToPakKey(const std::filesystem::path& filepath)
{
    if (!s_Pak || !s_Pak->IsOpen())
        return {};

    std::vector<std::string> parts;
    for (const auto& p : filepath) {
        std::string s = p.generic_string();
        if (s.empty() || s == "/" || s == "\\") continue;
        parts.push_back(std::move(s));
    }
    if (parts.empty())
        return {};

    // Longest trailing sub-path first.
    for (size_t start = 0; start < parts.size(); ++start) {
        std::string candidate;
        for (size_t i = start; i < parts.size(); ++i) {
            if (!candidate.empty()) candidate += '/';
            candidate += parts[i];
        }
        if (s_Pak->Has(candidate))
            return candidate;
    }
    return {};
}

// ─────────────────────────────────────────────────────────────────────────────

	Buffer FileSystem::ReadFileBinary(const std::filesystem::path& filepath)
	{
		// Check pak first — use Has() to distinguish "not in pak" from "zero-byte file in pak"
		if (s_Pak && s_Pak->IsOpen()) {
			auto key = ToPakKey(filepath);
			if (s_Pak->Has(key)) {
				auto bytes = s_Pak->ReadBinary(key);
				Buffer buf(bytes.empty() ? 0 : bytes.size());
				if (!bytes.empty())
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
		// Check pak first — use Has() so zero-byte files don't fall through
		if (s_Pak && s_Pak->IsOpen()) {
			auto key = ToPakKey(filepath);
			if (s_Pak->Has(key))
				return s_Pak->ReadText(key);
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
