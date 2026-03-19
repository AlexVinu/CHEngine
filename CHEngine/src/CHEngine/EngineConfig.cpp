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
        case ERenderAPI::OPENGL:  return "opengl";
        case ERenderAPI::VULKAN:  return "vulkan";
        case ERenderAPI::METAL:  return "metal";
        default:                  return "opengl";
        }
    }

    static ERenderAPI StringToApi(const std::string& s)
    {
        if (s == "vulkan") return ERenderAPI::VULKAN;
        if (s == "metal")  return ERenderAPI::METAL;
        return ERenderAPI::OPENGL;
    }

    static std::filesystem::path GetConfigPath()
    {
        return std::filesystem::current_path() / "engine.json";
    }

    static nlohmann::json ReadJsonSafe(const std::filesystem::path& path)
    {
        nlohmann::json j = nlohmann::json::object();
        if (std::filesystem::exists(path)) {
            try {
                std::ifstream ifs(path);
                ifs >> j;
            } catch (...) {}
        }
        return j;
    }

    static void WriteJson(const std::filesystem::path& path, const nlohmann::json& j)
    {
        std::ofstream ofs(path);
        ofs << j.dump(2) << std::endl;
    }

    ERenderAPI EngineConfig::LoadRendererPreference()
    {
        auto path = GetConfigPath();
        auto j = ReadJsonSafe(path);

        // Если есть pending — очищаем его из файла немедленно (до попытки загрузки).
        // Если движок крашнется при загрузке pending-рендерера, pending уже не будет
        // в файле → следующий запуск безопасно вернётся к последнему рабочему renderer.
        if (j.contains("renderer_pending") && j["renderer_pending"].is_string())
        {
            auto pending = StringToApi(j["renderer_pending"].get<std::string>());
            j.erase("renderer_pending");
            try {
                WriteJson(path, j);
                CHE_CORE_INFO("EngineConfig: pending renderer={} (cleared from config)", ApiToString(pending));
            }
            catch (const std::exception& e) {
                CHE_CORE_WARN("EngineConfig: failed to clear pending: {}", e.what());
            }
            // Проверяем платформу: если в конфиге Metal а мы на Windows — fallback
            if (IsPlatformSupported(pending))
                return pending;
            CHE_CORE_WARN("EngineConfig: pending renderer={} not supported on this platform, ignoring",
                          ApiToString(pending));
        }

        if (j.contains("renderer") && j["renderer"].is_string())
        {
            auto api = StringToApi(j["renderer"].get<std::string>());
            if (IsPlatformSupported(api))
                return api;
            CHE_CORE_WARN("EngineConfig: saved renderer={} not supported on this platform, falling back to OpenGL",
                          ApiToString(api));
        }

        return ERenderAPI::OPENGL;
    }

    void EngineConfig::SaveRendererPreference(ERenderAPI api)
    {
        // Пишем renderer_pending, а не renderer.
        // renderer (подтверждённый) обновится только в CommitRendererPreference,
        // которая вызывается после успешной инициализации движка.
        auto path = GetConfigPath();
        try {
            auto j = ReadJsonSafe(path);
            j["renderer_pending"] = ApiToString(api);
            WriteJson(path, j);
            CHE_CORE_INFO("EngineConfig: pending renderer={} saved (will commit on successful start)", ApiToString(api));
        }
        catch (const std::exception& e) {
            CHE_CORE_ERROR("EngineConfig: failed to save pending: {}", e.what());
        }
    }

    void EngineConfig::CommitRendererPreference(ERenderAPI api)
    {
        auto path = GetConfigPath();
        try {
            auto j = ReadJsonSafe(path);
            j["renderer"] = ApiToString(api);
            j.erase("renderer_pending"); // на случай если остался
            WriteJson(path, j);
            CHE_CORE_INFO("EngineConfig: committed renderer={}", ApiToString(api));
        }
        catch (const std::exception& e) {
            CHE_CORE_ERROR("EngineConfig: failed to commit renderer: {}", e.what());
        }
    }

    bool EngineConfig::IsPlatformSupported(ERenderAPI api)
    {
        switch (api) {
        case ERenderAPI::OPENGL:
            return true;
        case ERenderAPI::VULKAN:
#if defined(CHE_PLATFORM_APPLE)
            return false; // MoltenVK не поддерживается в текущей конфигурации
#else
            return true;
#endif
        case ERenderAPI::METAL:
#if defined(CHE_PLATFORM_APPLE)
            return true;
#else
            return false;
#endif
        case ERenderAPI::DIRECTX11:
        case ERenderAPI::DIRECTX12:
#if defined(CHE_PLATFORM_WINDOWS)
            return true;
#else
            return false;
#endif
        default:
            return false;
        }
    }

}
