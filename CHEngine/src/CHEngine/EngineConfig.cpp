#include "chepch.h"
#include "EngineConfig.h"

#include "Log/Log.h"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>

namespace CHEngine {

    static const char* ApiToString(ERenderAPI api)
    {
        switch (api) {
        case ERenderAPI::VULKAN:  return "vulkan";
        case ERenderAPI::METALL:  return "metal";
        default:                  return "opengl";
        }
    }

    static ERenderAPI StringToApi(const std::string& s)
    {
        if (s == "vulkan") return ERenderAPI::VULKAN;
        if (s == "metal")  return ERenderAPI::METALL;
        return ERenderAPI::OPENGL;
    }

    static std::filesystem::path GetConfigPath()
    {
        return std::filesystem::current_path() / "engine.json";
    }

    ERenderAPI EngineConfig::LoadRendererPreference()
    {
        auto path = GetConfigPath();
        if (!std::filesystem::exists(path))
            return ERenderAPI::OPENGL;

        try {
            std::ifstream ifs(path);
            nlohmann::json j;
            ifs >> j;
            if (j.contains("renderer") && j["renderer"].is_string())
                return StringToApi(j["renderer"].get<std::string>());
        }
        catch (const std::exception& e) {
            CHE_CORE_WARN("EngineConfig: failed to read {}: {}", path.string(), e.what());
        }
        return ERenderAPI::OPENGL;
    }

    void EngineConfig::SaveRendererPreference(ERenderAPI api)
    {
        auto path = GetConfigPath();
        try {
            nlohmann::json j;
            // Прочитать существующий файл, чтобы не затереть другие настройки
            if (std::filesystem::exists(path)) {
                std::ifstream ifs(path);
                try { ifs >> j; } catch (...) { j = nlohmann::json::object(); }
            }
            j["renderer"] = ApiToString(api);
            std::ofstream ofs(path);
            ofs << j.dump(2) << std::endl;
            CHE_CORE_INFO("EngineConfig: saved renderer={} to {}", ApiToString(api), path.string());
        }
        catch (const std::exception& e) {
            CHE_CORE_ERROR("EngineConfig: failed to save {}: {}", path.string(), e.what());
        }
    }

}
