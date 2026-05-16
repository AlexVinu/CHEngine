#include "chepch.h"
#include "PakFile.h"

#include <fstream>
#include <cstring>
#include <algorithm>

namespace CHEngine {

// ─────────────────────────────────────────────────────────────────────────────
// PakReader
// ─────────────────────────────────────────────────────────────────────────────

bool PakReader::Open(const std::filesystem::path& pakPath)
{
    Close();

    std::ifstream f(pakPath, std::ios::binary);
    if (!f) return false;

    // Read magic
    char magic[8];
    f.read(magic, 8);
    if (!f || std::memcmp(magic, kPakMagic, 8) != 0)
        return false;

    // Read header
    uint32_t version = 0, numEntries = 0;
    f.read(reinterpret_cast<char*>(&version),    4);
    f.read(reinterpret_cast<char*>(&numEntries), 4);
    f.read(reinterpret_cast<char*>(&m_DataOffset), 8);

    if (!f || version != kPakVersion) return false;

    // Read TOC
    m_Entries.reserve(numEntries);
    for (uint32_t i = 0; i < numEntries; ++i)
    {
        uint16_t pathLen = 0;
        f.read(reinterpret_cast<char*>(&pathLen), 2);
        if (!f || pathLen == 0) return false;

        std::string path(pathLen, '\0');
        f.read(path.data(), pathLen);

        uint64_t offset = 0, size = 0;
        f.read(reinterpret_cast<char*>(&offset), 8);
        f.read(reinterpret_cast<char*>(&size),   8);
        if (!f) return false;

        m_Index[path] = m_Entries.size();
        m_Entries.push_back({ std::move(path), offset, size });
    }

    m_PakPath = pakPath;
    m_Open    = true;
    return true;
}

void PakReader::Close()
{
    m_Open = false;
    m_Entries.clear();
    m_Index.clear();
    m_DataOffset = 0;
}

bool PakReader::Has(const std::string& relPath) const
{
    return m_Open && m_Index.count(relPath) > 0;
}

std::vector<uint8_t> PakReader::ReadBinary(const std::string& relPath) const
{
    if (!m_Open) return {};
    auto it = m_Index.find(relPath);
    if (it == m_Index.end()) return {};

    const PakEntry& e = m_Entries[it->second];
    if (e.size == 0) return {};

    std::ifstream f(m_PakPath, std::ios::binary);
    if (!f) return {};

    f.seekg(static_cast<std::streamoff>(m_DataOffset + e.offset));
    if (!f) return {};

    std::vector<uint8_t> buf(e.size);
    f.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(e.size));
    if (!f && !f.eof()) return {};

    return buf;
}

std::string PakReader::ReadText(const std::string& relPath) const
{
    auto bytes = ReadBinary(relPath);
    if (bytes.empty()) return {};
    return std::string(reinterpret_cast<char*>(bytes.data()), bytes.size());
}

// ─────────────────────────────────────────────────────────────────────────────
// PakBuilder
// ─────────────────────────────────────────────────────────────────────────────

void PakBuilder::AddFile(const std::string& relPath, const std::filesystem::path& absolutePath)
{
    // Normalise path separators to forward slash
    std::string normalised = relPath;
    std::replace(normalised.begin(), normalised.end(), '\\', '/');

    // Remove duplicate entry if exists
    auto it = std::find_if(m_Files.begin(), m_Files.end(),
        [&](const FileEntry& fe) { return fe.relPath == normalised; });
    if (it != m_Files.end())
        m_Files.erase(it);

    m_Files.push_back({ std::move(normalised), absolutePath });
}

void PakBuilder::AddDirectory(const std::filesystem::path& dir,
                               const std::string& relPathPrefix)
{
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec)) return;

    for (auto& entry : std::filesystem::recursive_directory_iterator(dir, ec))
    {
        if (!entry.is_regular_file()) continue;

        std::filesystem::path rel = std::filesystem::relative(entry.path(), dir, ec);
        if (ec) continue;

        std::string relStr = rel.generic_string(); // forward slashes
        if (!relPathPrefix.empty())
            relStr = relPathPrefix + "/" + relStr;

        AddFile(relStr, entry.path());
    }
}

bool PakBuilder::Write(const std::filesystem::path& outPath) const
{
    // Read all file data first to build offsets
    struct FileData {
        std::vector<uint8_t> bytes;
    };
    std::vector<FileData> data;
    data.reserve(m_Files.size());
    for (auto& fe : m_Files)
    {
        std::ifstream f(fe.absPath, std::ios::binary | std::ios::ate);
        if (!f) {
            data.push_back({});
            continue;
        }
        std::streamsize sz = f.tellg();
        f.seekg(0);
        std::vector<uint8_t> buf(sz > 0 ? static_cast<size_t>(sz) : 0);
        if (sz > 0) f.read(reinterpret_cast<char*>(buf.data()), sz);
        data.push_back({ std::move(buf) });
    }

    // Compute TOC size to find data offset
    // Header: 8 + 4 + 4 + 8 = 24 bytes
    // TOC entry: 2 + pathLen + 8 + 8
    uint64_t tocSize = 0;
    for (auto& fe : m_Files)
        tocSize += 2 + fe.relPath.size() + 8 + 8;

    const uint64_t dataOffset = 24 + tocSize;

    // Create parent dirs
    {
        std::error_code ec;
        std::filesystem::create_directories(outPath.parent_path(), ec);
    }

    std::ofstream f(outPath, std::ios::binary | std::ios::trunc);
    if (!f) return false;

    // Header
    f.write(kPakMagic, 8);
    uint32_t version    = kPakVersion;
    uint32_t numEntries = static_cast<uint32_t>(m_Files.size());
    f.write(reinterpret_cast<const char*>(&version),    4);
    f.write(reinterpret_cast<const char*>(&numEntries), 4);
    f.write(reinterpret_cast<const char*>(&dataOffset), 8);

    // TOC — compute offsets as we go
    uint64_t cursor = 0;
    for (size_t i = 0; i < m_Files.size(); ++i)
    {
        const std::string& p = m_Files[i].relPath;
        uint16_t pathLen = static_cast<uint16_t>(p.size());
        uint64_t sz      = data[i].bytes.size();
        f.write(reinterpret_cast<const char*>(&pathLen), 2);
        f.write(p.data(), pathLen);
        f.write(reinterpret_cast<const char*>(&cursor), 8);
        f.write(reinterpret_cast<const char*>(&sz),     8);
        cursor += sz;
    }

    // Data
    for (auto& fd : data)
        if (!fd.bytes.empty())
            f.write(reinterpret_cast<const char*>(fd.bytes.data()),
                    static_cast<std::streamsize>(fd.bytes.size()));

    return static_cast<bool>(f);
}

} // namespace CHEngine
