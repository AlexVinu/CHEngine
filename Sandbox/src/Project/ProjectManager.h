#pragma once
#include "Project.h"

#include <Core.h>
#include <CheStl/MemoryTypes.h>

#include <string>

class ProjectManager
{
public:
	ProjectManager();
	~ProjectManager();

    bool        Open(const std::string& projPath);
    std::string Create(const std::string& parentDir, const std::string& name);
    void        Close();
    Project*    Current();
    bool        HasProject();
	void Save() const;

	void AddRecentScene(const std::string& absolutePath);
private:
    Scope<Project> m_CurrentProject;

	static constexpr int k_MaxRecentScenes = 10;
	std::vector<std::string> m_RecentScenes; // relative to project root
};


