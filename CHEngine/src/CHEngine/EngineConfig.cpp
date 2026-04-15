#include "chepch.h"
#include "EngineConfig.h"

#include "FileSystem/FileSystem.h"
#include "Log/Log.h"

#include <nlohmann/json.hpp>
#include <filesystem>

namespace CHEngine {

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
        if (!FileSystem::Exists(path))
            return j;
        try {
            const std::string text = FileSystem::ReadFileText(path);
            if (text.empty())
                return j;
            return nlohmann::json::parse(text);
        } catch (...) {}
        return j;
    }

    static void WriteJson(const std::filesystem::path& path, const nlohmann::json& j)
    {
        const std::string dump = j.dump(2) + "\n";
        FileSystem::WriteFileText(path, dump);
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
                CHE_CORE_INFO("EngineConfig: pending renderer={} (cleared from config)", RenderModuleResolver::ToConfigName(pending));
            }
            catch (const std::exception& e) {
                CHE_CORE_WARN("EngineConfig: failed to clear pending: {}", e.what());
            }
            if (RenderModuleResolver::IsSupportedOnPlatform(pending))
                return pending;
            CHE_CORE_WARN("EngineConfig: pending renderer={} not supported on this platform, ignoring",
                          RenderModuleResolver::ToConfigName(pending));
        }

        if (j.contains("renderer") && j["renderer"].is_string())
        {
            auto api = StringToApi(j["renderer"].get<std::string>());
            if (RenderModuleResolver::IsSupportedOnPlatform(api))
                return api;
            CHE_CORE_WARN("EngineConfig: saved renderer={} not supported on this platform, falling back to OpenGL",
                          RenderModuleResolver::ToConfigName(api));
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
            j["renderer_pending"] = RenderModuleResolver::ToConfigName(api);
            WriteJson(path, j);
            CHE_CORE_INFO("EngineConfig: pending renderer={} saved (will commit on successful start)", RenderModuleResolver::ToConfigName(api));
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
            j["renderer"] = RenderModuleResolver::ToConfigName(api);
            j.erase("renderer_pending"); // на случай если остался
            WriteJson(path, j);
            CHE_CORE_INFO("EngineConfig: committed renderer={}", RenderModuleResolver::ToConfigName(api));
        }
        catch (const std::exception& e) {
            CHE_CORE_ERROR("EngineConfig: failed to commit renderer: {}", e.what());
        }
    }
}
