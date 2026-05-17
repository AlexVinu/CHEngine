#pragma once

// Original PakFile definitions - kept in Core/FileSystem
// Public API is re-exported via CHEngine/ResourceManager/AssetPack.h

#include "Core.h"

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>

namespace CHEngine {

static constexpr char     kPakMagic[8] = { 'C','H','E','P','A','K','0','1' };
static constexpr uint32_t kPakVersion  = 1;

struct PakEntry {
    std::string path;
    uint64_t    offset;
    uint64_t    size;
};

class CHE_CORE_API PakReader {
public:
    PakReader() = default;
    ~PakReader() = default;

    bool Open(const std::filesystem::path& pakPath);
    void Close();
    bool IsOpen() const { return m_Open; }
    bool Has(const std::string& relPath) const;
    std::vector<uint8_t> ReadBinary(const std::string& relPath) const;
    std::string ReadText(const std::string& relPath) const;
    const std::vector<PakEntry>& Entries() const { return m_Entries; }

private:
    bool           m_Open = false;
    std::filesystem::path m_PakPath;
    std::vector<PakEntry> m_Entries;
    std::unordered_map<std::string, size_t> m_Index;
    uint64_t       m_DataOffset = 0;
};

class CHE_CORE_API PakBuilder {
public:
    void AddFile(const std::string& relPath, const std::filesystem::path& absolutePath);
    void AddDirectory(const std::filesystem::path& dir, const std::string& relPathPrefix = "");
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
