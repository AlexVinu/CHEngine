#pragma once

#include <string>
#include <filesystem>
#include <functional>

// ─────────────────────────────────────────────────────────────────────────────
// ExportManager
//
// macOS:  создаёт <outputDir>/<gameTitle>.app  — двойной клик в Finder запускает
//   Contents/
//     Info.plist
//     MacOS/
//       GamePlayer  + все .dylib  (rpath @executable_path, работает без установки)
//     Resources/
//       game.chepak
//       game.json
//       shaders/
//
// Windows: создаёт <outputDir>/<gameTitle>/
//   GamePlayer.exe + *.dll + game.chepak + game.json + shaders/
// ─────────────────────────────────────────────────────────────────────────────

namespace Sandbox {

struct ExportConfig {
    std::filesystem::path projectDir;    // Корень CHEngine-проекта
    std::filesystem::path outputDir;     // Папка куда класть результат (не сам .app)
    std::filesystem::path playerBinary;  // Путь к собранному GamePlayer
    std::filesystem::path engineBinDir;  // Папка с dylib/dll движка
    std::string           startupScene;  // Относительный путь к стартовой сцене
    std::string           gameTitle    = "My Game";
    std::string           bundleId     = "com.chengine.game"; // macOS bundle identifier
    int                   windowWidth  = 1280;
    int                   windowHeight = 720;
};

using ProgressCallback = std::function<void(float progress, const std::string& message)>;

class ExportManager {
public:
    static bool Export(const ExportConfig& cfg, const ProgressCallback& progressCb = {});

    // Returns the final app/folder path that Export() will create
    static std::filesystem::path GetResultPath(const ExportConfig& cfg);

private:
    static bool PackAssets   (const ExportConfig& cfg, const std::filesystem::path& resourcesDir, const ProgressCallback& cb);
    static bool WriteGameJson(const ExportConfig& cfg, const std::filesystem::path& resourcesDir);
    static bool WritePlist   (const ExportConfig& cfg, const std::filesystem::path& contentsDir);
    static bool CopyBinaries (const ExportConfig& cfg, const std::filesystem::path& binDir);
};

} // namespace Sandbox
