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

static bool HasRendererArg(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        if (std::strncmp(argv[i], "--renderer=", 11) == 0)
            return true;
    }
    return false;
}

static CHEngine::ERenderAPI ParseRendererArg(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i)
    {
        if (std::strncmp(argv[i], "--renderer=", 11) == 0)
        {
            const char* val = argv[i] + 11;
            if (std::strcmp(val, "vulkan") == 0)  return CHEngine::ERenderAPI::VULKAN;
            if (std::strcmp(val, "metal") == 0)   return CHEngine::ERenderAPI::METAL;
            if (std::strcmp(val, "opengl") == 0)  return CHEngine::ERenderAPI::OPENGL;
        }
    }
    return CHEngine::ERenderAPI::OPENGL;
}

int main(int argc, char** argv)
{
    CHEngine::Log::init();
    CHE_CORE_INFO("Init CHEngine");
    CHE_CORE_CRITICAL("WELCOME TO HELL!");

    CHEngine::MemorySystem::Initialize();

    // Приоритет: CLI аргумент > engine.json > дефолт (OpenGL)
    CHEngine::ApplicationConfig config;
    if (HasRendererArg(argc, argv)) {
        auto requested = ParseRendererArg(argc, argv);
        if (CHEngine::EngineConfig::IsPlatformSupported(requested)) {
            config.RenderAPI = requested;
        } else {
            CHE_CORE_WARN("--renderer={}: not supported on this platform, using OpenGL",
                          (int)requested);
            config.RenderAPI = CHEngine::ERenderAPI::OPENGL;
        }
    } else {
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
