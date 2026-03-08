#pragma once

#include <Core.h>

#include "EngineContext.h"

namespace CHEngine
{
    class CHE_CORE_API MemorySystem
    {
    public:
        // Инициализация memory system
        static void Initialize();

        // Завершение работы (очистка)
        static void Shutdown();

        // Получить глобальный allocator
        static IAllocator* GetAllocator();

        // Можно добавить дополнительные методы, например set allocator
        static void SetAllocator(IAllocator* allocator);

    };
}