#pragma once

#include <Core.h>

#include "IAllocator.h"

namespace CHEngine
{
    struct CHE_CORE_API MemorySystem
    {
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
