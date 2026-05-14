#pragma once

#include <Core.h>
#include <Log/Log.h>

#define DECLARE_MODULE_FACTORY() \
extern "C" { \
    CHE_MODULE_API CHEngine::IModuleFactory* CreateFactory(); \
    CHE_MODULE_API void DestroyFactory(CHEngine::IModuleFactory*); \
}

#define IMPLEMENT_MODULE_FACTORY(FactoryType) \
    extern "C" \
{ \
    CHE_MODULE_API CHEngine::IModuleFactory* CreateFactory() \
    { \
        return new FactoryType(); \
    } \
    CHE_MODULE_API void DestroyFactory(CHEngine::IModuleFactory* factory) \
    { \
        delete factory; \
    } \
}

// Templates for unify creation
template<typename T, typename... Args>
T* CreateImpl(Args&&... args)
{
    CHE_MODULE_INFO("{0} CREATED", typeid(T).name());
    return new T(std::forward<Args>(args)...);
}

template<typename T>
void DestroyImpl(T* ptr)
{
    CHE_MODULE_INFO("{0} DELETED", typeid(T).name());
    delete ptr;
}

namespace CHEngine {
    // ВАЖНО: всегда задавать явные значения.
    // Без них добавление нового элемента в середину списка сдвигает все остальные,
    // и скомпилированные DLL начинают возвращать неверный тип (как это уже случалось).
    enum class ModuleType
    {
        Window        = 0,
        WindowHandler = 1,
        Render        = 2,
        ImGui         = 3,
        Physics       = 4,
        None          = 5
    };

    struct IModuleFactory
    {
        virtual ~IModuleFactory() = default;

        virtual ModuleType GetType() const = 0;
    };
}

DECLARE_MODULE_FACTORY()
