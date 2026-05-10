#pragma once

#include <Core.h>

#include <filesystem>
#include <functional>
#include <vector>

namespace CHEngine {

// ─── FileWatcher ──────────────────────────────────────────────────────────────
// Кроссплатформенный наблюдатель за изменениями файлов на диске.
// Работает через polling (опрос last_write_time) — без OS-специфичных API.
// Вызывать Poll() раз в несколько кадров, не каждый кадр.
class CHENGINE_API FileWatcher
{
public:
    using Callback = std::function<void(const std::filesystem::path&)>;

    // Начать следить за файлом. Callback вызывается при изменении.
    // Если файл не существует на момент вызова — выдаёт предупреждение и не регистрирует.
    void Watch(const std::filesystem::path& path, Callback callback);

    // Опросить все файлы — вызывать раз в 0.5–1 сек, не каждый кадр.
    void Poll();

    // Снять все наблюдения.
    void Clear();

private:
    struct WatchEntry
    {
        std::filesystem::path           Path;
        std::filesystem::file_time_type LastWrite;
        Callback                        OnChanged;
    };

    std::vector<WatchEntry> m_Entries;
};

} // namespace CHEngine
