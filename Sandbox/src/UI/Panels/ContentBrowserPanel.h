#pragma once
#include <string>
#include <filesystem>
#include <vector>
#include <imgui.h>

namespace fs = std::filesystem;

class ContentBrowserPanel {
public:
    ContentBrowserPanel();

    // Renders the panel. Returns a non-empty action string when user selects a file:
    //   "model:<path>"  — user wants to import a 3D model
    //   "scene:<path>"  — user wants to open a .chscene file
    //   "script:<path>" — user wants to open a .lua script
    //   "shader:<path>" — user wants to open a .slang/.glsl shader
    // Returns empty string if no action.
    std::string OnImGuiRender(ImVec2 pos, ImVec2 size);

    void SetAssetsDirectory(const fs::path& path);

private:
    void DrawDirectoryTree(const fs::path& dir, int depth = 0);
    void DrawFileGrid();
    const char* GetFileIcon(const fs::path& path) const;
    bool IsModelFile(const fs::path& path)  const;
    bool IsSceneFile(const fs::path& path)  const;
    bool IsScriptFile(const fs::path& path) const;
    bool IsShaderFile(const fs::path& path) const;

    fs::path    m_AssetsRoot;
    fs::path    m_CurrentDir;
    std::string m_PendingAction;

    float m_TreeWidth = 180.0f;
};
