#include "chepch.h"
#include "RecentFiles.h"

#include "FileSystem/FileSystem.h"

#include <algorithm>
#include <sstream>

namespace CHEngine {

void RecentFiles::AddPath(const std::string& path) {
    // Remove if already present (to move it to front)
    m_Paths.erase(std::remove(m_Paths.begin(), m_Paths.end(), path), m_Paths.end());
    m_Paths.insert(m_Paths.begin(), path);
    if ((int)m_Paths.size() > k_Max)
        m_Paths.resize(k_Max);
}

bool RecentFiles::SaveToFile(const std::string& filePath) const {
    std::ostringstream oss;
    for (auto& p : m_Paths) oss << p << "\n";
    return FileSystem::WriteFileText(filePath, oss.str());
}

bool RecentFiles::LoadFromFile(const std::string& filePath) {
    if (!FileSystem::Exists(filePath))
        return false;
    const std::string text = FileSystem::ReadFileText(filePath);
    m_Paths.clear();
    std::istringstream iss(text);
    std::string line;
    while (std::getline(iss, line) && (int)m_Paths.size() < k_Max)
        if (!line.empty()) m_Paths.push_back(line);
    return true;
}

} // namespace CHEngine
