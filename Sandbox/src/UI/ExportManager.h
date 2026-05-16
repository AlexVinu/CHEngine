#pragma once

#include <string>
#include <filesystem>
#include <functional>

// ─────────────────────────────────────────────────────────────────────────────
// ExportManager
//
// Packages a CHEngine project into a distributable folder:
//   <outputDir>/
//     GamePlayer  (or GamePlayer.exe on Windows)
//     *.dylib / *.dll
//     shaders/
//     game.chepak   — all project assets (scenes, scripts, textures, models)
//     game.json     — startup config (startup scene, window title, etc.)
//
// Usage:
//   ExportConfig cfg;
//   cfg.projectDir  = "/Users/marat/Projects/MyGame";
//   cfg.outputDir   = "/Users/marat/Desktop/MyGame_export";
//   cfg.startupScene = "Scenes/main.chscene";
//   cfg.playerBinary = "/path/to/GamePlayer";
//   ExportManager::Export(cfg, [](float p, const std::string& msg){ ... });
// ─────────────────────────────────────────────────────────────────────────────

namespace Sandbox {

struct ExportConfig {
    std::filesystem::path projectDir;      // Root of the CHEngine project
    std::filesystem::path outputDir;       // Where to write the export
    std::filesystem::path playerBinary;    // Path to built GamePlayer binary
    std::filesystem::path engineBinDir;    // Directory containing engine dylibs
    std::string           startupScene;    // Relative path to startup .chscene
    std::string           gameTitle       = "My Game";
    int                   windowWidth     = 1280;
    int                   windowHeight    = 720;
};

using ProgressCallback = std::function<void(float progress, const std::string& message)>;

class ExportManager {
public:
    // Performs the full export. Calls progressCb periodically with [0..1].
    // Returns true on success. Errors logged to CHE_CORE_ERROR.
    static bool Export(const ExportConfig& cfg, const ProgressCallback& progressCb = {});

private:
    static bool PackAssets(const ExportConfig& cfg, const ProgressCallback& cb);
    static bool WriteGameJson(const ExportConfig& cfg);
    static bool CopyBinaries(const ExportConfig& cfg);
};

} // namespace Sandbox
