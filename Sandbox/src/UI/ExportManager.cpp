#include "ExportManager.h"

#include <FileSystem/PakFile.h>
#include <Log/Log.h>

#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <cstring>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace Sandbox {

// ─────────────────────────────────────────────────────────────────────────────
bool ExportManager::Export(const ExportConfig& cfg, const ProgressCallback& progressCb)
{
    auto progress = [&](float p, const std::string& msg) {
        CHE_CORE_INFO("[Export] {}", msg);
        if (progressCb) progressCb(p, msg);
    };

    // Validate
    if (!fs::exists(cfg.projectDir)) {
        CHE_CORE_ERROR("[Export] Project directory not found: {}", cfg.projectDir.string());
        return false;
    }
    if (!fs::exists(cfg.playerBinary)) {
        CHE_CORE_ERROR("[Export] Player binary not found: {}", cfg.playerBinary.string());
        return false;
    }

    // Create output dir
    std::error_code ec;
    fs::create_directories(cfg.outputDir, ec);
    if (ec) {
        CHE_CORE_ERROR("[Export] Cannot create output dir: {}", ec.message());
        return false;
    }

    progress(0.0f, "Упаковка ассетов...");
    if (!PackAssets(cfg, progressCb)) return false;

    progress(0.7f, "Копирование бинарников...");
    if (!CopyBinaries(cfg)) return false;

    progress(0.9f, "Запись game.json...");
    if (!WriteGameJson(cfg)) return false;

    progress(1.0f, "Экспорт завершён → " + cfg.outputDir.string());
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
bool ExportManager::PackAssets(const ExportConfig& cfg, const ProgressCallback& cb)
{
    CHEngine::PakBuilder builder;

    // Directories to pack (relative paths inside pak match relative paths inside projectDir)
    struct DirEntry { fs::path absDir; std::string pakPrefix; };
    std::vector<DirEntry> dirs = {
        { cfg.projectDir / "Assets",  "Assets"  },
        { cfg.projectDir / "Scenes",  "Scenes"  },
        { cfg.projectDir / "assets",  "assets"  },  // lowercase fallback
        { cfg.projectDir / "scenes",  "scenes"  },
    };

    // Also pack shaders from engine bin dir
    dirs.push_back({ cfg.engineBinDir / "shaders", "shaders" });

    for (auto& d : dirs) {
        if (!fs::exists(d.absDir)) continue;
        builder.AddDirectory(d.absDir, d.pakPrefix);
    }

    if (builder.FileCount() == 0) {
        CHE_CORE_WARN("[Export] No assets found to pack — pak will be empty");
    }

    fs::path pakOut = cfg.outputDir / "game.chepak";
    if (!builder.Write(pakOut)) {
        CHE_CORE_ERROR("[Export] Failed to write pak: {}", pakOut.string());
        return false;
    }

    CHE_CORE_INFO("[Export] Packed {} files → {}", builder.FileCount(), pakOut.string());
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
bool ExportManager::WriteGameJson(const ExportConfig& cfg)
{
    json j;
    j["title"]         = cfg.gameTitle;
    j["startup_scene"] = cfg.startupScene;
    j["width"]         = cfg.windowWidth;
    j["height"]        = cfg.windowHeight;

    fs::path out = cfg.outputDir / "game.json";
    std::ofstream f(out);
    if (!f) {
        CHE_CORE_ERROR("[Export] Cannot write game.json: {}", out.string());
        return false;
    }
    f << j.dump(4);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
bool ExportManager::CopyBinaries(const ExportConfig& cfg)
{
    std::error_code ec;

    // Copy player binary
    fs::path playerDst = cfg.outputDir / cfg.playerBinary.filename();
    fs::copy_file(cfg.playerBinary, playerDst,
                  fs::copy_options::overwrite_existing, ec);
    if (ec) {
        CHE_CORE_ERROR("[Export] Cannot copy player binary: {}", ec.message());
        return false;
    }

#ifndef _WIN32
    // Make executable — use filesystem::permissions instead of std::system(chmod)
    // to avoid shell injection if the path contains special characters.
    fs::permissions(playerDst,
        fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
        fs::perm_options::add, ec);
    if (ec)
        CHE_CORE_WARN("[Export] Could not set executable bit: {}", ec.message());
#endif

    // Copy all dylibs/dlls from engine bin dir to output dir
    if (!cfg.engineBinDir.empty() && fs::exists(cfg.engineBinDir))
    {
        for (auto& entry : fs::directory_iterator(cfg.engineBinDir, ec))
        {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().string();
            if (ext != ".dylib" && ext != ".dll" && ext != ".so") continue;

            fs::path dst = cfg.outputDir / entry.path().filename();
            fs::copy_file(entry.path(), dst,
                          fs::copy_options::overwrite_existing, ec);
            if (ec)
                CHE_CORE_WARN("[Export] Could not copy {}: {}", entry.path().filename().string(), ec.message());
        }
    }

    // Copy shaders dir (fallback if not in pak)
    fs::path shadersDir = cfg.engineBinDir / "shaders";
    if (fs::exists(shadersDir))
    {
        fs::path shadersDst = cfg.outputDir / "shaders";
        fs::create_directories(shadersDst, ec);
        if (ec) {
            CHE_CORE_WARN("[Export] Cannot create shaders dir: {}", ec.message());
        } else {
            fs::copy(shadersDir, shadersDst,
                     fs::copy_options::overwrite_existing | fs::copy_options::recursive, ec);
            if (ec)
                CHE_CORE_WARN("[Export] Shader copy incomplete: {}", ec.message());
        }
    }

    return true;
}

} // namespace Sandbox
