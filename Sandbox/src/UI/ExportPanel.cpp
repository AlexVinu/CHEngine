#include "ExportPanel.h"

#include <CHEngine/Utils/AppPaths.h>
#include <CHEngine/EngineConfig.h>
#include <Log/Log.h>

#include <filesystem>
#include <cstring>

namespace fs = std::filesystem;

namespace Sandbox {

void ExportPanel::Open()
{
    m_Open     = true;
    m_Done     = false;
    m_Success  = false;
    m_Progress = 0.0f;
    m_StatusMsg.clear();

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

    ImGui::Text("Выходная папка");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##outdir", m_OutputDir, sizeof(m_OutputDir));

    ImGui::Text("Название игры");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##title", m_GameTitle, sizeof(m_GameTitle));

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

    if (m_Exporting) {
        std::lock_guard<std::mutex> lk(m_Mutex);
        ImGui::ProgressBar(m_Progress, ImVec2(-1, 0));
        ImGui::TextWrapped("%s", m_StatusMsg.c_str());
    } else if (m_Done) {
        if (m_Success) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.9f, 0.3f, 1.0f));
            ImGui::TextUnformatted("✓ Экспорт завершён успешно!");
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            ImGui::TextUnformatted("✗ Ошибка экспорта. Смотри логи.");
            ImGui::PopStyleColor();
        }
        ImGui::TextWrapped("%s", m_StatusMsg.c_str());
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

    // Poll worker completion
    if (m_Exporting)
    {
        std::lock_guard<std::mutex> lk(m_Mutex);
        if (m_Done) m_Exporting = false;
    }

    ImGui::End();
}

// ─────────────────────────────────────────────────────────────────────────────
void ExportPanel::StartExport(SceneViewLayerHost& host)
{
    if (m_Worker.joinable()) m_Worker.join();

    auto session = host.GetActiveSceneSession();
    std::string startupScene;
    if (session) startupScene = session->SceneRelPath;
    if (startupScene.empty()) startupScene = "Scenes/main.chscene";

    // Build config
    ExportConfig cfg;
    cfg.outputDir    = m_OutputDir;
    cfg.gameTitle    = m_GameTitle;
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
    std::string lastProj = CHEngine::EngineConfig::LoadLastProject();
    if (!lastProj.empty())
        cfg.projectDir = fs::path(lastProj).parent_path();
    else
        cfg.projectDir = CHEngine::AppPaths::ExecutableDir();

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
            ? "Готово: " + cfg.outputDir.string()
            : "Экспорт упал. Смотри логи.";
    });
}

} // namespace Sandbox
