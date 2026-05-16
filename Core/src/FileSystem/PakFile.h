#pragma once

#include "Core.h"

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>

namespace CHEngine {

// ─────────────────────────────────────────────────────────────────────────────
// .chepak binary format
//
// [Header]  8 bytes magic + 4 version + 4 numEntries + 8 dataOffset
// [TOC]     numEntries * (2 pathLen + n path + 8 offset + 8 size)
// [Data]    raw file bytes concatenated
//
// All paths stored as relative, forward-slash separated: "scenes/main.chscene"
// ─────────────────────────────────────────────────────────────────────────────

static constexpr char    kPakMagic[8]   = { 'C','H','E','P','A','K','0','1' };
static constexpr uint32_t kPakVersion   = 1;

struct PakEntry {
    std::string path;    // relative path (forward slashes)
    uint64_t    offset;  // byte offset inside data section
    uint64_t    size;    // byte size
};

// ─────────────────────────────────────────────────────────────────────────────
// PakReader — mounts a .chepak and provides random-access reads
// ─────────────────────────────────────────────────────────────────────────────
class CHE_CORE_API PakReader {
public:
    PakReader() = default;
    ~PakReader() = default;

    bool Open(const std::filesystem::path& pakPath);
    void Close();
    bool IsOpen() const { return m_Open; }

    // Returns true if the path exists in this pak (forward-slash, case-sensitive)
    bool Has(const std::string& relPath) const;

    // Read file as bytes. Returns empty buffer on failure.
    std::vector<uint8_t> ReadBinary(const std::string& relPath) const;

    // Read file as UTF-8 text. Returns empty string on failure.
    std::string ReadText(const std::string& relPath) const;

    const std::vector<PakEntry>& Entries() const { return m_Entries; }

private:
    bool           m_Open = false;
    std::filesystem::path               m_PakPath;
    std::vector<PakEntry>               m_Entries;
    std::unordered_map<std::string, size_t> m_Index; // path → index in m_Entries
    uint64_t       m_DataOffset = 0; // byte offset in pak file where data starts
};

// ─────────────────────────────────────────────────────────────────────────────
// PakBuilder — collects files and writes a .chepak
// ─────────────────────────────────────────────────────────────────────────────
class CHE_CORE_API PakBuilder {
public:
    // Add a single file. relPath = how it will be accessed at runtime.
    void AddFile(const std::string& relPath, const std::filesystem::path& absolutePath);

    // Recursively add all files from a directory.
    // relPathPrefix is prepended to each file's relative path.
    void AddDirectory(const std::filesystem::path& dir,
                      const std::string& relPathPrefix = "");

    // Write the pak to disk. Returns true on success.
    bool Write(const std::filesystem::path& outPath) const;

    size_t FileCount() const { return m_Files.size(); }

private:
    struct FileEntry {
        std::string           relPath;
        std::filesystem::path absPath;
    };
    std::vector<FileEntry> m_Files;
};

} // namespace CHEngine
