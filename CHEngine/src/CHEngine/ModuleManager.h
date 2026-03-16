#pragma once
#include <Core.h>

#include <functional>
#include <string>
#include <unordered_map>
#include <utility>

#include "IModuleFactory.h"
#include "CHEngine/Utils/FileWatcher.h"

// Platform-specific dynamic library includes (must be outside namespace)
#ifdef CHE_PLATFORM_WINDOWS
    #include <Windows.h>
#elif defined(CHE_PLATFORM_LINUX) || defined(CHE_PLATFORM_APPLE)
    #include <dlfcn.h>
#endif

namespace CHEngine
{
    using DestroyFn    = void(*)(IModuleFactory*);
    using CreateFn     = IModuleFactory*(*)();

    #ifdef CHE_PLATFORM_WINDOWS
        using ModuleHandle = HMODULE;
    #elif defined(CHE_PLATFORM_LINUX) || defined(CHE_PLATFORM_APPLE)
        using ModuleHandle = void*;
    #else
        #error Unsupported platform
    #endif

    // ─── Колбэки для горячей перезагрузки модуля ─────────────────────────────
    // OnBeforeReload — уничтожить все объекты, созданные старым модулем
    // OnAfterReload  — пересоздать объекты через новую фабрику
    struct ModuleReloadCallbacks
    {
        std::function<void()>                OnBeforeReload;
        std::function<void(IModuleFactory*)> OnAfterReload;
    };

    class ModuleManager
    {
    public:

        ~ModuleManager() { UnloadAll(); }

        bool LoadModule(const std::string& path)
        {
            ModuleHandle handle = Load(path.c_str());
            if (!handle)
            {
            #if defined(CHE_PLATFORM_LINUX) || defined(CHE_PLATFORM_APPLE)
                CHE_CORE_ERROR("LoadModule FAILED '{}': {}", path, dlerror());
            #else
                CHE_CORE_ERROR("LoadModule FAILED '{}'", path);
            #endif
                return false;
            }

            auto create = reinterpret_cast<CreateFn>(
                GetSymbol(handle, "CreateFactory"));

            auto destroy = reinterpret_cast<DestroyFn>(
                GetSymbol(handle, "DestroyFactory"));

            if (!create || !destroy)
            {
                CHE_CORE_ERROR("LoadModule '{}': CreateFactory/DestroyFactory symbol not found", path);
                return false;
            }

            IModuleFactory* module = create();
            ModuleType type = module->GetType();

            m_Modules[type] = { handle, module, destroy, path };

            return true;
        }

        // Зарегистрировать hot reload для модуля.
        // Вызывать ПОСЛЕ LoadModule — следит за dylib-файлом через FileWatcher.
        void Watch(ModuleType type, ModuleReloadCallbacks callbacks)
        {
            auto it = m_Modules.find(type);
            if (it == m_Modules.end())
            {
                CHE_CORE_WARN("ModuleManager::Watch — модуль типа {} не загружен", (int)type);
                return;
            }

            m_Callbacks[type] = std::move(callbacks);

            const std::string& path = it->second.path;
            m_Watcher.Watch(std::filesystem::path(path),
                [this, type](const std::filesystem::path&) { ReloadModule(type); });
        }

        // Опросить FileWatcher — вызывать раз в 1 сек из Application::Run().
        void Poll() { m_Watcher.Poll(); }

        void UnloadAll()
        {
            m_Watcher.Clear();
            m_Callbacks.clear();
            for (auto& [type, data] : m_Modules)
            {
                data.destroy(data.module);
                Unload(data.handle);
            }
            m_Modules.clear();
        }

        template<typename T>
        T* GetModule(ModuleType type)
        {
            auto it = m_Modules.find(type);
            if (it == m_Modules.end())
            {
                CHE_CORE_ERROR("ModuleManager: модуль типа {} не найден", (int)type);
                return nullptr;
            }

            return static_cast<T*>(it->second.module);
        }

    private:
        struct ModuleData
        {
            ModuleHandle    handle;
            IModuleFactory* module;
            DestroyFn       destroy;
            std::string     path;   // путь к dylib для перезагрузки
        };

        std::unordered_map<ModuleType, ModuleData>           m_Modules;
        std::unordered_map<ModuleType, ModuleReloadCallbacks> m_Callbacks;
        FileWatcher                                           m_Watcher;

        // ─── Горячая перезагрузка одного модуля ──────────────────────────────
        void ReloadModule(ModuleType type)
        {
            auto it = m_Modules.find(type);
            if (it == m_Modules.end()) return;

            std::string path = it->second.path; // сохраняем до удаления

            CHE_CORE_INFO("ModuleManager: перезагрузка модуля типа {}...", (int)type);

            // 1. Дать возможность Application уничтожить зависимые объекты
            auto cbIt = m_Callbacks.find(type);
            if (cbIt != m_Callbacks.end() && cbIt->second.OnBeforeReload)
                cbIt->second.OnBeforeReload();

            // 2. Выгрузить старый модуль
            it->second.destroy(it->second.module);
            Unload(it->second.handle);
            m_Modules.erase(it);

            // 3. Загрузить новый
            if (!LoadModule(path))
            {
                CHE_CORE_ERROR("ModuleManager: не удалось перезагрузить '{}' — модуль недоступен", path);
                return;
            }

            // 4. Уведомить Application о новой фабрике
            if (cbIt != m_Callbacks.end() && cbIt->second.OnAfterReload)
                cbIt->second.OnAfterReload(m_Modules[type].module);

            CHE_CORE_INFO("ModuleManager: модуль '{}' перезагружен успешно", path);
        }

        ModuleHandle Load(const char* path)
        {
        #if defined(CHE_PLATFORM_WINDOWS)
            return LoadLibraryA(path);
        #else
            return dlopen(path, RTLD_NOW);
        #endif
        }

        void* GetSymbol(ModuleHandle handle, const char* name)
        {
        #if defined(CHE_PLATFORM_WINDOWS)
            return reinterpret_cast<void*>(
                GetProcAddress(handle, name));
        #else
            return dlsym(handle, name);
        #endif
        }

        void Unload(ModuleHandle handle)
        {
        #if defined(CHE_PLATFORM_WINDOWS)
            FreeLibrary(handle);
        #else
            dlclose(handle);
        #endif
        }
    };
}
