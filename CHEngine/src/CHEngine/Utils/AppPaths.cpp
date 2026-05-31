#include "chepch.h"
#include "AppPaths.h"

#include <cstdlib>

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

namespace {

std::filesystem::path ResolveUserDataRoot()
{
#if defined(_WIN32)
    if (const char* localAppData = std::getenv("LOCALAPPDATA"))
        return std::filesystem::path(localAppData);
#elif defined(__APPLE__)
    if (const char* home = std::getenv("HOME"))
        return std::filesystem::path(home) / "Library" / "Application Support";
#elif defined(__linux__)
    if (const char* xdg = std::getenv("XDG_DATA_HOME"))
        return std::filesystem::path(xdg);
    if (const char* home = std::getenv("HOME"))
        return std::filesystem::path(home) / ".local" / "share";
#endif
    return {};
}

// Strip path separators and other characters that aren't safe in a directory name.
std::string SanitizeTitle(const std::string& title)
{
    std::string out;
    out.reserve(title.size());
    for (char c : title)
    {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
            c == '"' || c == '<' || c == '>' || c == '|')
            out += '_';
        else
            out += c;
    }
    if (out.empty())
        out = "CHEngineGame";
    return out;
}

} // namespace

std::filesystem::path AppPaths::UserDataDir(const std::string& gameTitle)
{
    const std::string safe = SanitizeTitle(gameTitle);

    std::filesystem::path root = ResolveUserDataRoot();
    std::filesystem::path dir = root.empty()
        ? (ExecutableDir() / "saves" / safe)
        : (root / safe);

    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}

} // namespace CHEngine
