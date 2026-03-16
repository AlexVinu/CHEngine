#include "chepch.h"
#include "FileWatcher.h"

#include <Log/Log.h>

namespace CHEngine {

void FileWatcher::Watch(const std::filesystem::path& path, Callback callback)
{
    if (!std::filesystem::exists(path)) {
        CHE_CORE_WARN("FileWatcher: файл не найден, наблюдение не начато: {0}", path.string().c_str());
        return;
    }

    auto lastWrite = std::filesystem::last_write_time(path);
    m_Entries.push_back({ path, lastWrite, std::move(callback) });

    CHE_CORE_TRACE("FileWatcher: слежу за '{0}'", path.filename().string().c_str());
}

void FileWatcher::Poll()
{
    for (auto& entry : m_Entries) {
        if (!std::filesystem::exists(entry.Path))
            continue;

        auto currentWrite = std::filesystem::last_write_time(entry.Path);
        if (currentWrite != entry.LastWrite) {
            entry.LastWrite = currentWrite;
            CHE_CORE_INFO("FileWatcher: изменён '{0}' — перезагрузка...", entry.Path.filename().string().c_str());
            if (entry.OnChanged)
                entry.OnChanged(entry.Path);
        }
    }
}

void FileWatcher::Clear()
{
    m_Entries.clear();
}

} // namespace CHEngine
