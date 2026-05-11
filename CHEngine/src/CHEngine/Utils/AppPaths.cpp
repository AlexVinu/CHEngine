#include "chepch.h"
#include "AppPaths.h"

#if defined(_WIN32)
    #include <windows.h>
#elif defined(__APPLE__)
    #include <mach-o/dyld.h>
#elif defined(__linux__)
    #include <unistd.h>
    #include <limits.h>
#endif

namespace CHEngine {

static std::filesystem::path ResolveExecutableDir()
{
#if defined(_WIN32)
    wchar_t buf[MAX_PATH];
    DWORD len = ::GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (len == 0 || len == MAX_PATH)
        return std::filesystem::current_path();
    return std::filesystem::path(buf).parent_path();
#elif defined(__APPLE__)
    char buf[PATH_MAX];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) != 0)
        return std::filesystem::current_path();
    std::error_code ec;
    auto canonical = std::filesystem::canonical(std::filesystem::path(buf), ec);
    return ec ? std::filesystem::path(buf).parent_path() : canonical.parent_path();
#elif defined(__linux__)
    char buf[PATH_MAX];
    ssize_t len = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len <= 0)
        return std::filesystem::current_path();
    buf[len] = '\0';
    return std::filesystem::path(buf).parent_path();
#else
    return std::filesystem::current_path();
#endif
}

const std::filesystem::path& AppPaths::ExecutableDir()
{
    static const std::filesystem::path s_Dir = ResolveExecutableDir();
    return s_Dir;
}

std::filesystem::path AppPaths::ShadersDir()
{
    return ExecutableDir() / "shaders";
}

std::filesystem::path AppPaths::EditorAssetsDir()
{
    return ExecutableDir() / "editor_assets";
}

std::filesystem::path AppPaths::EngineConfigPath()
{
    return ExecutableDir() / "engine.json";
}

} // namespace CHEngine
