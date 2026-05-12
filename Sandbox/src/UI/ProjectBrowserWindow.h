#pragma once
#include <string>

/// Полноэкранное окно выбора/создания проекта.
/// Показывается вместо основного редактора пока ProjectManager::HasProject() == false.
/// Draw() возвращает true в тот кадр, когда проект был открыт/создан.
class ProjectBrowserWindow
{
public:
    bool Draw();

private:
    std::string DrawRecentList(const std::vector<std::string>& recents);
    bool TryOpenProject(const std::string& path);
    bool StartNewProject();

    enum class Step { Idle, EnterName };
    Step        m_Step       = Step::Idle;
    char        m_NameBuf[256] = "MyProject";
    std::string m_ParentDir;
    bool        m_OpenNamePopup = false;
};
