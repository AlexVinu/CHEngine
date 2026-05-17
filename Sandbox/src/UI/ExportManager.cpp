#include "ExportManager.h"

#include <FileSystem/PakFile.h>
#include <Log/Log.h>

#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace Sandbox {

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static std::string SafeTitle(const std::string& title)
{
    std::string s;
    for (char c : title)
        s += (c == ' ' || c == '/' || c == '\\' || c == ':') ? '_' : c;
    return s.empty() ? "Game" : s;
}

std::filesystem::path ExportManager::GetResultPath(const ExportConfig& cfg)
{
#ifdef __APPLE__
    return cfg.outputDir / (SafeTitle(cfg.gameTitle) + ".app");
#elif _WIN32
    return cfg.outputDir / SafeTitle(cfg.gameTitle);
#else
    return cfg.outputDir / SafeTitle(cfg.gameTitle);
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
bool ExportManager::Export(const ExportConfig& cfg, const ProgressCallback& progressCb)
{
    auto progress = [&](float p, const std::string& msg) {
        CHE_CORE_INFO("[Export] {}", msg);
        if (progressCb) progressCb(p, msg);
    };

    if (!fs::exists(cfg.projectDir)) {
        CHE_CORE_ERROR("[Export] Project dir not found: {}", cfg.projectDir.string());
        return false;
    }
    if (!fs::exists(cfg.playerBinary)) {
        CHE_CORE_ERROR("[Export] Player binary not found: {}", cfg.playerBinary.string());
        return false;
    }

    std::error_code ec;

    // ── Determine directory layout ────────────────────────────────────────────
    fs::path result = GetResultPath(cfg);

#ifdef __APPLE__
    // macOS .app bundle structure
    fs::path contentsDir  = result / "Contents";
    fs::path macosDir     = contentsDir / "MacOS";    // binary + dylibs
    fs::path resourcesDir = contentsDir / "Resources"; // assets
#else
    // Windows / Linux: flat folder
    fs::path contentsDir  = result;
    fs::path macosDir     = result;
    fs::path resourcesDir = result;
#endif

    for (auto& d : { result, contentsDir, macosDir, resourcesDir }) {
        fs::create_directories(d, ec);
        if (ec) {
            CHE_CORE_ERROR("[Export] Cannot create dir {}: {}", d.string(), ec.message());
            return false;
        }
    }

    progress(0.05f, "Упаковка ассетов в .chepak...");
    if (!PackAssets(cfg, resourcesDir, progressCb)) return false;

    progress(0.60f, "Копирование бинарника и библиотек...");
    if (!CopyBinaries(cfg, macosDir)) return false;

    progress(0.85f, "Запись конфига...");
    if (!WriteGameJson(cfg, resourcesDir)) return false;

#ifdef __APPLE__
    progress(0.90f, "Создание Info.plist...");
    if (!WritePlist(cfg, contentsDir)) return false;
#endif

    progress(1.0f, "Готово → " + result.string());
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
bool ExportManager::PackAssets(const ExportConfig& cfg,
                                const fs::path& resourcesDir,
                                const ProgressCallback& cb)
{
    (void)cb;
    CHEngine::PakBuilder builder;

    struct DirEntry { fs::path absDir; std::string pakPrefix; };
    std::vector<DirEntry> dirs = {
        { cfg.projectDir / "Assets",  "Assets"  },
        { cfg.projectDir / "Scenes",  "Scenes"  },
        { cfg.projectDir / "assets",  "assets"  },
        { cfg.projectDir / "scenes",  "scenes"  },
    };
    dirs.push_back({ cfg.engineBinDir / "shaders", "shaders" });

    for (auto& d : dirs) {
        if (!fs::exists(d.absDir)) continue;
        builder.AddDirectory(d.absDir, d.pakPrefix);
    }

    if (builder.FileCount() == 0)
        CHE_CORE_WARN("[Export] No assets found — pak will be empty");

    fs::path pakOut = resourcesDir / "game.chepak";
    if (!builder.Write(pakOut)) {
        CHE_CORE_ERROR("[Export] Failed to write pak: {}", pakOut.string());
        return false;
    }
    CHE_CORE_INFO("[Export] Packed {} files → {}", builder.FileCount(), pakOut.string());
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
bool ExportManager::WriteGameJson(const ExportConfig& cfg, const fs::path& resourcesDir)
{
    json j;
    j["title"]         = cfg.gameTitle;
    j["startup_scene"] = cfg.startupScene;
    j["width"]         = cfg.windowWidth;
    j["height"]        = cfg.windowHeight;

    fs::path out = resourcesDir / "game.json";
    std::ofstream f(out);
    if (!f) {
        CHE_CORE_ERROR("[Export] Cannot write game.json: {}", out.string());
        return false;
    }
    f << j.dump(4);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
bool ExportManager::WritePlist(const ExportConfig& cfg, const fs::path& contentsDir)
{
    // Minimal Info.plist for macOS .app recognition
    std::string plist = R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
    "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key>
    <string>)" + cfg.gameTitle + R"(</string>
    <key>CFBundleExecutable</key>
    <string>GamePlayer</string>
    <key>CFBundleIdentifier</key>
    <string>)" + cfg.bundleId + R"(</string>
    <key>CFBundleVersion</key>
    <string>1.0</string>
    <key>CFBundleShortVersionString</key>
    <string>1.0</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleSignature</key>
    <string>????</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>NSPrincipalClass</key>
    <string>NSApplication</string>
</dict>
</plist>
)";

    fs::path out = contentsDir / "Info.plist";
    std::ofstream f(out);
    if (!f) {
        CHE_CORE_ERROR("[Export] Cannot write Info.plist: {}", out.string());
        return false;
    }
    f << plist;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
bool ExportManager::CopyBinaries(const ExportConfig& cfg, const fs::path& binDir)
{
    std::error_code ec;

    // Copy player executable
    fs::path playerDst = binDir / cfg.playerBinary.filename();
    fs::copy_file(cfg.playerBinary, playerDst, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        CHE_CORE_ERROR("[Export] Cannot copy player: {}", ec.message());
        return false;
    }

#ifndef _WIN32
    fs::permissions(playerDst,
        fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
        fs::perm_options::add, ec);
    if (ec)
        CHE_CORE_WARN("[Export] Could not set executable bit: {}", ec.message());
#endif

    // Copy dylibs/dlls — they go next to the executable so @executable_path works
    if (!cfg.engineBinDir.empty() && fs::exists(cfg.engineBinDir))
    {
        for (auto& entry : fs::directory_iterator(cfg.engineBinDir, ec))
        {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().string();
            if (ext != ".dylib" && ext != ".dll" && ext != ".so") continue;

            fs::path dst = binDir / entry.path().filename();
            fs::copy_file(entry.path(), dst, fs::copy_options::overwrite_existing, ec);
            if (ec)
                CHE_CORE_WARN("[Export] Could not copy {}: {}",
                    entry.path().filename().string(), ec.message());
        }
    }

    return true;
}

} // namespace Sandbox
