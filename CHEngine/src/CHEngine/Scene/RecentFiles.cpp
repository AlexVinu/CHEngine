#include "chepch.h"
#include "RecentFiles.h"
#include <fstream>
#include <algorithm>

namespace CHEngine {

void RecentFiles::AddPath(const std::string& path) {
    // Remove if already present (to move it to front)
    m_Paths.erase(std::remove(m_Paths.begin(), m_Paths.end(), path), m_Paths.end());
    m_Paths.insert(m_Paths.begin(), path);
    if ((int)m_Paths.size() > k_Max)
        m_Paths.resize(k_Max);
}

bool RecentFiles::SaveToFile(const std::string& filePath) const {
    std::ofstream f(filePath);
    if (!f) return false;
    for (auto& p : m_Paths) f << p << "\n";
    return true;
}

bool RecentFiles::LoadFromFile(const std::string& filePath) {
    std::ifstream f(filePath);
    if (!f) return false;
    m_Paths.clear();
    std::string line;
    while (std::getline(f, line) && (int)m_Paths.size() < k_Max)
        if (!line.empty()) m_Paths.push_back(line);
    return true;
}

} // namespace CHEngine
