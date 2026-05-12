#pragma once
#include "Project.h"
#include <Core.h>
#include <string>

namespace CHEngine {

class CHENGINE_API ProjectManager
{
    ProjectManager() = delete;
    ~ProjectManager() = delete;
public:
    static bool        Open(const std::string& projPath);
    static std::string Create(const std::string& parentDir, const std::string& name);
    static void        Close();
    static Project*    Current();
    static bool        HasProject();
};

} // namespace CHEngine
