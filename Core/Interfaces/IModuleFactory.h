#pragma once

#include <Core.h>

#define DECLARE_MODULE_FACTORY() \
extern "C" { \
    CHE_MODULE_API CHEngine::IModuleFactory* CreateFactory(); \
    CHE_MODULE_API void DestroyFactory(CHEngine::IModuleFactory*); \
}

namespace CHEngine {
    enum class ModuleType
    {
        Render, Physics, None
    };

    struct IModuleFactory
    {
        virtual ~IModuleFactory() = default;

        virtual ModuleType GetType() const = 0;
    };
}