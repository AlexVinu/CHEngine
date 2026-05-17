#pragma once

#include <cstring>
#include <cstdlib>
#include "EngineConfig.h"

#if defined(CHE_PLATFORM_APPLE)
#include <unistd.h>
#include <mach-o/dyld.h>
#elif defined(CHE_PLATFORM_LINUX)
#include <unistd.h>
#include <climits>
#elif defined(CHE_PLATFORM_WINDOWS)
#include <process.h>
#include <Windows.h>
#endif

extern CHEngine::Application* CHEngine::CreateApplication(const CHEngine::ApplicationConfig& config);

static bool StartsWith(const char* value, const char* prefix)
{
    return std::strncmp(value, prefix, std::strlen(prefix)) == 0;
}

static bool ParseBoolArg(const char* value, bool& out_value)
{
    if (std::strcmp(value, "on") == 0 || std::strcmp(value, "true") == 0 || std::strcmp(value, "1") == 0)
    {
        out_value = true;
        return true;
    }

    if (std::strcmp(value, "off") == 0 || std::strcmp(value, "false") == 0 || std::strcmp(value, "0") == 0)
    {
        out_value = false;
        return true;
    }

    return false;
}

static bool ParseRendererValue(const char* value, CHEngine::ERenderAPI* out_api)
{
    return CHEngine::RenderModuleResolver::TryParseRenderAPI(value, out_api);
}

static void ParseStartupArgs(int argc, char** argv, CHEngine::ApplicationConfig& config, bool& has_renderer_override)
{
    has_renderer_override = false;

    for (int i = 1; i < argc; ++i)
    {
        const char* arg = argv[i];

        if (StartsWith(arg, "--renderer="))
        {
            const char* value = arg + std::strlen("--renderer=");
            CHEngine::ERenderAPI requested = CHEngine::ERenderAPI::OPENGL;
            if (ParseRendererValue(value, &requested))
            {
                has_renderer_override = true;
                if (CHEngine::RenderModuleResolver::IsSupportedOnPlatform(requested))
                {
                    config.RenderAPI = requested;
                }
                else
                {
                    CHE_CORE_WARN("--renderer={} is not supported on this platform, using OpenGL", value);
                    config.RenderAPI = CHEngine::ERenderAPI::OPENGL;
                }
            }
            else
            {
                CHE_CORE_WARN("Unknown --renderer value '{}', keeping current value", value);
            }
            continue;
        }

        if (StartsWith(arg, "--window-module="))
        {
            config.WindowModuleOverride = arg + std::strlen("--window-module=");
            continue;
        }

        if (StartsWith(arg, "--renderer-module="))
        {
            config.RendererModuleOverride = arg + std::strlen("--renderer-module=");
            continue;
        }

        if (StartsWith(arg, "--imgui-module="))
        {
            config.ImGuiModuleOverride = arg + std::strlen("--imgui-module=");
            continue;
        }

        if (StartsWith(arg, "--physics-module="))
        {
            config.PhysicsModuleOverride = arg + std::strlen("--physics-module=");
            continue;
        }

        if (StartsWith(arg, "--physics="))
        {
            const char* value = arg + std::strlen("--physics=");
            bool physics_enabled = config.PhysicsEnabled;
            if (ParseBoolArg(value, physics_enabled))
            {
                config.PhysicsEnabled = physics_enabled;
            }
            else
            {
                CHE_CORE_WARN("Unknown --physics value '{}', expected on|off|true|false|1|0", value);
            }
            continue;
        }

        if (StartsWith(arg, "--"))
        {
            CHE_CORE_WARN("Unknown startup argument '{}'", arg);
        }
    }
}

int main(int argc, char** argv)
{
    // EngineContext is a Meyers-singleton: the first CHE_CORE_* call below
    // triggers GetEngineContext() which constructs the singleton and runs
    // EngineContext::EngineContext() — that initialises spdlog and the default
    // allocator.  Explicit Log::init() / MemorySystem::Initialize() are no
    // longer needed here.
    CHE_CORE_INFO("Init CHEngine");
    CHE_CORE_CRITICAL("WELCOME TO HELL!");

    // Приоритет: CLI аргумент > engine.json > дефолт (OpenGL)
    CHEngine::ApplicationConfig config;
    bool hasRendererOverride = false;
    ParseStartupArgs(argc, argv, config, hasRendererOverride);
    if (!hasRendererOverride) {
        config.RenderAPI = CHEngine::EngineConfig::LoadRendererPreference();
    }

    auto app = CHEngine::CreateApplication(config);
    app->Run();

    bool restart = app->IsRestartRequested();
    delete app;

    if (restart) {
        // argv[0] может быть относительным, а Application меняет cwd —
        // нужен абсолютный путь к executable.
#if defined(CHE_PLATFORM_APPLE)
        char exePath[4096];
        uint32_t exeSize = sizeof(exePath);
        if (_NSGetExecutablePath(exePath, &exeSize) == 0) {
            char* resolved = realpath(exePath, nullptr);
            if (resolved) {
                execv(resolved, argv);
                // execv возвращается только при ошибке — процесс не заменён
                perror("execv failed");
                free(resolved);
                return 1;
            }
        }
        perror("restart failed: cannot resolve exe path");
        return 1;
#elif defined(CHE_PLATFORM_LINUX)
        char exePath[PATH_MAX];
        ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
        if (len > 0) {
            exePath[len] = '\0';
            execv(exePath, argv);
        }
        perror("execv failed");
        return 1;
#elif defined(CHE_PLATFORM_WINDOWS)
        char exePath[MAX_PATH];
        if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) > 0) {
            _execv(exePath, argv);
        }
        return 1;
#endif
    }

    return 0;
}
