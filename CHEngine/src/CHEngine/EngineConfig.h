#pragma once

#include <PlatformAPICapabilities.h>

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
    };

}
