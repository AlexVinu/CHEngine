#include "ContentBrowserPanel.h"
#include <imgui.h>
#include <algorithm>
#include <CHEngine/Project/ProjectManager.h>
#include <Log/Log.h>

ContentBrowserPanel::ContentBrowserPanel() {
    if (CHEngine::ProjectManager::HasProject())
        SetAssetsDirectory(CHEngine::ProjectManager::Current()->AssetsAbsPath());
    // Without a project the browser stays empty until OnProjectOpened sets the root.
}

void ContentBrowserPanel::SetAssetsDirectory(const fs::path& path) {
    m_AssetsRoot = path;
    m_CurrentDir = path;
}

bool ContentBrowserPanel::IsModelFile(const fs::path& path) const {
    auto ext = path.extension().string();
    return ext == ".obj" || ext == ".glb" || ext == ".gltf";
}

bool ContentBrowserPanel::IsSceneFile(const fs::path& path) const {
    return path.extension().string() == ".chscene";
}

bool ContentBrowserPanel::IsScriptFile(const fs::path& path) const {
    return path.extension().string() == ".lua";
}

bool ContentBrowserPanel::IsShaderFile(const fs::path& path) const {
    auto ext = path.extension().string();
    return ext == ".slang" || ext == ".glsl" || ext == ".hlsl";
}

const char* ContentBrowserPanel::GetFileIcon(const fs::path& path) const {
    if (fs::is_directory(path))    return "[DIR]";
    if (IsModelFile(path))         return "[3D] ";
    if (IsSceneFile(path))         return "[SCN]";
    if (IsScriptFile(path))        return "[LUA]";
    if (IsShaderFile(path))        return "[SHD]";
    auto ext = path.extension().string();
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga")
        return "[IMG]";
    return "[   ]";
}

void ContentBrowserPanel::DrawDirectoryTree(const fs::path& dir, int depth) {
    if (!fs::exists(dir) || !fs::is_directory(dir)) return;

    // Collect entries
    std::vector<fs::directory_entry> entries;
    try {
        for (auto& e : fs::directory_iterator(dir))
            if (fs::is_directory(e)) entries.push_back(e);
    } catch (const std::exception& e) {
        CHE_CORE_WARN("ContentBrowser: cannot read directory '{}': {}", dir.string(), e.what());
        return;
    }

    std::sort(entries.begin(), entries.end(),
        [](const fs::directory_entry& a, const fs::directory_entry& b) {
            return a.path().filename() < b.path().filename();
        });

    for (auto& entry : entries) {
        std::string name = entry.path().filename().string();
        if (name.empty() || name[0] == '.') continue;

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                                   ImGuiTreeNodeFlags_SpanFullWidth;
        if (entry.path() == m_CurrentDir)
            flags |= ImGuiTreeNodeFlags_Selected;

        bool open = ImGui::TreeNodeEx(name.c_str(), flags);
        if (ImGui::IsItemClicked())
            m_CurrentDir = entry.path();

        if (open) {
            DrawDirectoryTree(entry.path(), depth + 1);
            ImGui::TreePop();
        }
    }
}

void ContentBrowserPanel::DrawFileGrid() {
    if (!fs::exists(m_CurrentDir)) return;

    // Collect all entries in current directory
    std::vector<fs::directory_entry> dirs, files;
    try {
        for (auto& e : fs::directory_iterator(m_CurrentDir)) {
            std::string name = e.path().filename().string();
            if (name.empty() || name[0] == '.') continue;
            if (fs::is_directory(e)) dirs.push_back(e);
            else files.push_back(e);
        }
    } catch (const std::exception& e) {
        CHE_CORE_WARN("ContentBrowser: cannot list directory '{}': {}", m_CurrentDir.string(), e.what());
        return;
    }

    // Sort both
    auto byName = [](const fs::directory_entry& a, const fs::directory_entry& b) {
        return a.path().filename() < b.path().filename();
    };
    std::sort(dirs.begin(),  dirs.end(),  byName);
    std::sort(files.begin(), files.end(), byName);

    // Combine: dirs first
    std::vector<fs::directory_entry> all;
    all.insert(all.end(), dirs.begin(),  dirs.end());
    all.insert(all.end(), files.begin(), files.end());

    // Breadcrumb
    {
        fs::path rel = fs::relative(m_CurrentDir, m_AssetsRoot.parent_path());
        std::vector<fs::path> parts;
        for (const auto& part : rel) parts.push_back(part);

        fs::path built = m_AssetsRoot.parent_path();
        for (size_t i = 0; i < parts.size(); i++) {
            built /= parts[i];
            if (i > 0) { ImGui::SameLine(0, 2); ImGui::TextDisabled("/"); ImGui::SameLine(0, 2); }
            if (ImGui::SmallButton(parts[i].string().c_str()))
                m_CurrentDir = built;
        }
        ImGui::Separator();
    }

    if (all.empty()) {
        ImGui::TextDisabled("  (empty)");
        return;
    }

    // Grid with 5 columns
    const int cols = 5;
    float cellW = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ScrollbarSize) / cols - 4.0f;
    if (cellW < 60.0f) cellW = 60.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4, 4));
    if (ImGui::BeginTable("##files", cols, ImGuiTableFlags_None)) {
        for (auto& entry : all) {
            ImGui::TableNextColumn();

            std::string name = entry.path().filename().string();
            const char* icon = GetFileIcon(entry.path());
            bool isDir    = fs::is_directory(entry.path());
            bool isModel  = !isDir && IsModelFile(entry.path());
            bool isScene  = !isDir && IsSceneFile(entry.path());
            bool isScript = !isDir && IsScriptFile(entry.path());
            bool isShader = !isDir && IsShaderFile(entry.path());

            // Truncate long names
            std::string display = name;
            if (display.size() > 14) display = display.substr(0, 11) + "...";

            std::string id = "##entry_" + name;

            // Selectable block
            ImGui::BeginGroup();
            ImGui::Selectable(id.c_str(), false,
                ImGuiSelectableFlags_None, ImVec2(cellW, 52.0f));

            bool dblClicked = ImGui::IsItemHovered() &&
                              ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

            // Overlay icon + name on top of the selectable
            ImVec2 p  = ImGui::GetItemRectMin();
            ImDrawList* dl = ImGui::GetWindowDrawList();

            // Icon text centered
            ImVec2 iconPos = { p.x + cellW * 0.5f - ImGui::CalcTextSize(icon).x * 0.5f, p.y + 6.0f };
            dl->AddText(iconPos,
                isModel  ? IM_COL32(100,180,255,255) :
                isScene  ? IM_COL32(255,200, 80,255) :
                isScript ? IM_COL32( 80,220,120,255) :
                isShader ? IM_COL32(200,130,255,255) :
                isDir    ? IM_COL32(220,200,100,255) :
                           IM_COL32(180,180,180,255), icon);

            // Name text centered
            ImVec2 namePos = { p.x + cellW * 0.5f - ImGui::CalcTextSize(display.c_str()).x * 0.5f, p.y + 30.0f };
            dl->AddText(namePos, IM_COL32(200,200,200,255), display.c_str());

            ImGui::EndGroup();

            if (dblClicked) {
                if (isDir)
                    m_CurrentDir = entry.path();
                else if (isModel)
                    m_PendingAction = "model:"  + entry.path().string();
                else if (isScene)
                    m_PendingAction = "scene:"  + entry.path().string();
                else if (isScript)
                    m_PendingAction = "script:" + entry.path().string();
                else if (isShader)
                    m_PendingAction = "shader:" + entry.path().string();
            }

            // Tooltip with full name if truncated
            if (ImGui::IsItemHovered() && name.size() > 14)
                ImGui::SetTooltip("%s", name.c_str());
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

std::string ContentBrowserPanel::OnImGuiRender(ImVec2 pos, ImVec2 size) {
    m_PendingAction.clear();

    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(size);
    ImGui::SetNextWindowBgAlpha(1.0f);
    ImGui::Begin("Content Browser", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    if (!CHEngine::ProjectManager::HasProject() || m_AssetsRoot.empty()) {
        ImGui::TextDisabled("CONTENT");
        ImGui::Separator();
        ImGui::TextDisabled("  No project open.");
        ImGui::End();
        return m_PendingAction;
    }

    // Header bar
    ImGui::TextDisabled("CONTENT");
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    // Root folder shortcut
    {
        std::string rootName = m_AssetsRoot.filename().string();
        if (rootName.empty()) rootName = m_AssetsRoot.string(); // path like "C:\" or "/"
        if (rootName.empty()) rootName = "root";
        if (ImGui::SmallButton(rootName.c_str()))
            m_CurrentDir = m_AssetsRoot;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", m_AssetsRoot.string().c_str());
    }
    ImGui::Separator();

    // Two columns: directory tree (left) + file grid (right)
    float treeW = m_TreeWidth;
    ImGui::BeginChild("##tree", ImVec2(treeW, 0), true);

    // Root node
    ImGuiTreeNodeFlags rootFlags = ImGuiTreeNodeFlags_DefaultOpen |
                                   ImGuiTreeNodeFlags_OpenOnArrow;
    if (m_CurrentDir == m_AssetsRoot) rootFlags |= ImGuiTreeNodeFlags_Selected;
    bool rootOpen = ImGui::TreeNodeEx("assets", rootFlags);
    if (ImGui::IsItemClicked()) m_CurrentDir = m_AssetsRoot;
    if (rootOpen) {
        DrawDirectoryTree(m_AssetsRoot);
        ImGui::TreePop();
    }

    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##grid", ImVec2(0, 0), false);
    DrawFileGrid();
    ImGui::EndChild();

    ImGui::End();
    return m_PendingAction;
}
