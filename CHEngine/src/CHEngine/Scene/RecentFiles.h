#pragma once
#include <Core.h>
#include <string>
#include <vector>

namespace CHEngine {

class CHENGINE_API RecentFiles {
public:
    static constexpr int k_Max = 5;

    void AddPath(const std::string& path);
    void Clear() { m_Paths.clear(); }
    const std::vector<std::string>& GetPaths() const { return m_Paths; }

    bool SaveToFile(const std::string& filePath) const;
    bool LoadFromFile(const std::string& filePath);

private:
    std::vector<std::string> m_Paths;
};

} // namespace CHEngine
