#pragma once

#include <RenderData.h>

namespace CHEngine {

    struct CHENGINE_API EngineConfig
    {
        /// Читает renderer preference из engine.json (рядом с executable).
        /// Если файл не найден или невалиден — возвращает ERenderAPI::OPENGL.
        static ERenderAPI LoadRendererPreference();

        /// Сохраняет renderer preference в engine.json.
        static void SaveRendererPreference(ERenderAPI api);
    };

}
