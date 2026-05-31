#include "ExportPanel.h"

#include <CHEngine/Utils/AppPaths.h>
#include <CHEngine/EngineConfig.h>
#include <Log/Log.h>

#include <filesystem>
#include <cstring>

namespace fs = std::filesystem;

namespace Sandbox {

// Resolve the project root from the last saved project file.
static fs::path ResolveProjectDir()
{
    std::string lastProj = CHEngine::EngineConfig::LoadLastProject();
    if (!lastProj.empty())
        return fs::path(lastProj).parent_path();
    return CHEngine::AppPaths::ExecutableDir();
}

// Scan <project>/Scenes (and lowercase variant) for .chscene files; store
// relative pak paths like "Scenes/main.chscene".
void ExportPanel::RefreshScenes()
{
    m_Scenes.clear();
    m_SceneSel = -1;

    fs::path projectDir = ResolveProjectDir();
    std::error_code ec;

    struct DirEntry { fs::path absDir; std::string pakPrefix; };
    const DirEntry candidates[] = {
        { projectDir / "Scenes", "Scenes" },
        { projectDir / "scenes", "scenes" },
    };

    for (const auto& d : candidates) {
        if (!fs::exists(d.absDir, ec)) continue;
        for (auto& entry : fs::directory_iterator(d.absDir, ec)) {
            if (ec) break;
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() != ".chscene") continue;
            m_Scenes.push_back(d.pakPrefix + "/" + entry.path().filename().generic_string());
        }
    }

    m_ScenesScanned = true;
    if (!m_Scenes.empty())
        m_SceneSel = 0;
}

void ExportPanel::Open()
{
    m_Open     = true;
    m_Done     = false;
    m_Success  = false;
    m_Progress = 0.0f;
    m_StatusMsg.clear();
    m_ScenesScanned = false;
    RefreshScenes();

    // Pre-fill output dir: Desktop/GameExport
    if (m_OutputDir[0] == '\0') {
        const char* home = std::getenv("HOME");
        if (home) {
            std::string def = std::string(home) + "/Desktop/GameExport";
            std::strncpy(m_OutputDir, def.c_str(), sizeof(m_OutputDir) - 1);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void ExportPanel::Draw(SceneViewLayerHost& host)
{
    if (!m_Open) return;

    ImGui::SetNextWindowSize(ImVec2(520, 320), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(
        ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f - 260,
               ImGui::GetIO().DisplaySize.y * 0.5f - 160),
        ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Экспорт проекта", &m_Open,
                      ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }

    // Settings
    ImGui::SeparatorText("Настройки");

    ImGui::Text("Куда сохранить");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##outdir", m_OutputDir, sizeof(m_OutputDir));

    ImGui::Text("Название игры");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##title", m_GameTitle, sizeof(m_GameTitle)))
        {}  // live preview updates below

    // Startup scene picker
    ImGui::Text("Стартовая сцена");
    ImGui::SameLine();
    if (ImGui::SmallButton("⟳"))
        RefreshScenes();

    // On first open default to the active session's scene if present in the list.
    if (m_ScenesScanned) {
        if (auto session = host.GetActiveSceneSession(); session && !session->SceneRelPath.empty()) {
            for (size_t i = 0; i < m_Scenes.size(); ++i) {
                if (m_Scenes[i] == session->SceneRelPath) { m_SceneSel = static_cast<int>(i); break; }
            }
        }
        m_ScenesScanned = false;  // apply default selection only once per scan
    }

    ImGui::SetNextItemWidth(-1);
    const char* preview = (m_SceneSel >= 0 && m_SceneSel < static_cast<int>(m_Scenes.size()))
        ? m_Scenes[m_SceneSel].c_str()
        : "<нет сцен>";
    if (ImGui::BeginCombo("##startupscene", preview)) {
        for (int i = 0; i < static_cast<int>(m_Scenes.size()); ++i) {
            bool selected = (i == m_SceneSel);
            if (ImGui::Selectable(m_Scenes[i].c_str(), selected))
                m_SceneSel = i;
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

#ifdef __APPLE__
    // Show what will be created
    if (m_OutputDir[0] && m_GameTitle[0]) {
        std::string preview = std::string(m_OutputDir) + "/" + m_GameTitle + ".app";
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.8f, 0.5f, 1.0f));
        ImGui::TextWrapped("→ %s", preview.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::Text("Bundle ID");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##bundleid", m_BundleId, sizeof(m_BundleId));
    ImGui::SetItemTooltip("com.company.gamename — нужен macOS для идентификации приложения");
#endif

    ImGui::Text("Разрешение окна");
    ImGui::SetNextItemWidth(100);
    ImGui::InputInt("##w", &m_Width, 0);
    ImGui::SameLine();
    ImGui::TextUnformatted("×");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::InputInt("##h", &m_Height, 0);

    ImGui::Spacing();
    ImGui::SeparatorText("Статус");

    // Snapshot shared state under mutex, then render outside the lock.
    float       snapProgress = 0.0f;
    std::string snapStatus;
    bool        snapDone     = false;
    bool        snapSuccess  = false;
    {
        std::lock_guard<std::mutex> lk(m_Mutex);
        snapProgress = m_Progress;
        snapStatus   = m_StatusMsg;
        snapDone     = m_Done;
        snapSuccess  = m_Success;
        if (m_Done) m_Exporting = false;  // transition while locked
    }

    if (m_Exporting) {
        ImGui::ProgressBar(snapProgress, ImVec2(-1, 0));
        ImGui::TextWrapped("%s", snapStatus.c_str());
    } else if (snapDone) {
        if (snapSuccess) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.9f, 0.3f, 1.0f));
            ImGui::TextUnformatted("✓ Экспорт завершён успешно!");
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            ImGui::TextUnformatted("✗ Ошибка экспорта. Смотри логи.");
            ImGui::PopStyleColor();
        }
        ImGui::TextWrapped("%s", snapStatus.c_str());
    } else {
        ImGui::TextDisabled("Настрой параметры и нажми Экспортировать");
    }

    ImGui::Spacing();
    ImGui::Separator();

    bool canExport = !m_Exporting && m_OutputDir[0] != '\0';
    if (!canExport) ImGui::BeginDisabled();
    if (ImGui::Button("Экспортировать", ImVec2(160, 0)))
        StartExport(host);
    if (!canExport) ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Закрыть", ImVec2(80, 0)))
        m_Open = false;

    // Completion already handled in the snapshot block above.

    ImGui::End();
}

// ─────────────────────────────────────────────────────────────────────────────
void ExportPanel::StartExport(SceneViewLayerHost& host)
{
    if (m_Worker.joinable()) m_Worker.join();

    std::string startupScene;
    if (m_SceneSel >= 0 && m_SceneSel < static_cast<int>(m_Scenes.size()))
        startupScene = m_Scenes[m_SceneSel];
    if (startupScene.empty()) {
        auto session = host.GetActiveSceneSession();
        if (session) startupScene = session->SceneRelPath;
    }
    if (startupScene.empty()) startupScene = "Scenes/main.chscene";

    // Build config
    ExportConfig cfg;
    cfg.outputDir    = m_OutputDir;
    cfg.gameTitle    = m_GameTitle;
    cfg.bundleId     = m_BundleId;
    cfg.windowWidth  = m_Width;
    cfg.windowHeight = m_Height;
    cfg.startupScene = startupScene;

    // Player binary: look for GamePlayer next to us
    fs::path playerBin = CHEngine::AppPaths::ExecutableDir() /
#ifdef _WIN32
        "../CHEngine_Player/GamePlayer.exe";
#else
        "../CHEngine_Player/GamePlayer";
#endif
    if (!fs::exists(playerBin))
        playerBin = CHEngine::AppPaths::ExecutableDir() / "GamePlayer";
    cfg.playerBinary = playerBin;

    // Engine bin dir = where we are
    cfg.engineBinDir = CHEngine::AppPaths::ExecutableDir();

    // Project dir from last saved project
    cfg.projectDir = ResolveProjectDir();

    m_Exporting = true;
    m_Done      = false;
    m_Success   = false;
    m_Progress  = 0.0f;
    m_StatusMsg = "Запуск...";

    m_Worker = std::thread([this, cfg]()
    {
        bool ok = ExportManager::Export(cfg, [this](float p, const std::string& msg) {
            std::lock_guard<std::mutex> lk(m_Mutex);
            m_Progress  = p;
            m_StatusMsg = msg;
        });
        std::lock_guard<std::mutex> lk(m_Mutex);
        m_Success = ok;
        m_Done    = true;
        m_StatusMsg = ok
            ? "Готово: " + ExportManager::GetResultPath(cfg).string()
            : "Экспорт упал. Смотри логи.";
    });
}

} // namespace Sandbox
