#pragma once

#include <PlatformAPICapabilities.h>
#include <string>
#include <vector>

namespace CHEngine {

    struct CHENGINE_API EngineConfig
    {
        /// Читает renderer из engine.json.
        /// Если есть renderer_pending — очищает его из файла и возвращает.
        /// Иначе возвращает renderer (последний подтверждённый), дефолт OPENGL.
        static ERenderAPI LoadRendererPreference();

        /// Записывает renderer_pending в engine.json (не renderer!).
        /// Фактически применяется только если следующий запуск успешен.
        static void SaveRendererPreference(ERenderAPI api);

        /// Записывает renderer в engine.json как подтверждённый.
        /// Вызывать после успешной инициализации всех модулей.
        static void CommitRendererPreference(ERenderAPI api);

        // ── Project ────────────────────────────────────────────────────────────

        /// Возвращает путь к последнему открытому проекту (пустую строку если нет).
        static std::string LoadLastProject();

        /// Сохраняет путь к последнему открытому проекту в engine.json.
        static void SaveLastProject(const std::string& path);

        /// Возвращает список недавних проектов (не более k_MaxRecentProjects).
        static std::vector<std::string> LoadRecentProjects();

        /// Добавляет путь в список recent_projects в engine.json.
        static void AddRecentProject(const std::string& path);

        static constexpr int k_MaxRecentProjects = 10;
    };

}
