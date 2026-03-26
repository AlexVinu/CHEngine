#pragma once
#include <Core.h>

#include <filesystem>
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
            std::string shadow;

        #ifdef CHE_PLATFORM_WINDOWS
            // ─── Shadow copy (только Windows) ─────────────────────────────
            // Windows блокирует загруженную DLL — нельзя перезаписать.
            // Копируем файл во временный и загружаем копию.
            // Оригинал остаётся свободным для перезаписи компилятором.
            shadow = MakeShadowPath(path);
            std::error_code ec;
            std::filesystem::copy_file(
                path, shadow,
                std::filesystem::copy_options::overwrite_existing, ec);

            if (ec) {
                CHE_CORE_ERROR("LoadModule: failed to create shadow copy '{}' -> '{}': {}",
                               path, shadow, ec.message());
                return false;
            }
        #endif

            // На macOS/Linux загружаем оригинал напрямую — ОС не блокирует
            // загруженные .so/.dylib, shadow copy не нужна.
            const std::string& loadPath = shadow.empty() ? path : shadow;

            ModuleHandle handle = Load(loadPath.c_str());
            if (!handle)
            {
            #if defined(CHE_PLATFORM_LINUX) || defined(CHE_PLATFORM_APPLE)
                CHE_CORE_ERROR("LoadModule FAILED '{}': {}", loadPath, dlerror());
            #else
                CHE_CORE_ERROR("LoadModule FAILED '{}'", loadPath);
            #endif
                // Удаляем неудачную теневую копию (если она была создана)
                if (!shadow.empty()) {
                    std::error_code rmEc;
                    std::filesystem::remove(shadow, rmEc);
                }
                return false;
            }

            auto create = reinterpret_cast<CreateFn>(
                GetSymbol(handle, "CreateFactory"));

            auto destroy = reinterpret_cast<DestroyFn>(
                GetSymbol(handle, "DestroyFactory"));

            if (!create || !destroy)
            {
                CHE_CORE_ERROR("LoadModule '{}': CreateFactory/DestroyFactory symbol not found", path);
                Unload(handle);
                if (!shadow.empty()) {
                    std::error_code rmEc;
                    std::filesystem::remove(shadow, rmEc);
                }
                return false;
            }

            IModuleFactory* module = create();
            if (!module) {
                CHE_CORE_ERROR("LoadModule '{}': CreateFactory() returned null", path);
                Unload(handle);
                if (!shadow.empty()) {
                    std::error_code rmEc;
                    std::filesystem::remove(shadow, rmEc);
                }
                return false;
            }
            ModuleType type = module->GetType();

            m_Modules[type] = { handle, module, destroy, path, shadow };

            return true;
        }

        // Зарегистрировать hot reload для модуля.
        // Вызывать ПОСЛЕ LoadModule — следит за dylib-файлом через FileWatcher.
        void Watch(ModuleType type, ModuleReloadCallbacks callbacks)
        {
            auto it = m_Modules.find(type);
            if (it == m_Modules.end())
            {
                CHE_CORE_WARN("ModuleManager::Watch - module type {} is not loaded", (int)type);
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
                // Удаляем теневую копию после выгрузки
                if (!data.shadowPath.empty()) {
                    std::error_code ec;
                    std::filesystem::remove(data.shadowPath, ec);
                }
            }
            m_Modules.clear();
        }

        template<typename T>
        T* GetModule(ModuleType type)
        {
            auto it = m_Modules.find(type);
            if (it == m_Modules.end())
            {
                CHE_CORE_ERROR("ModuleManager: module type {} not found", (int)type);
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
            std::string     path;       // путь к оригинальной dylib (для мониторинга)
            std::string     shadowPath; // путь к теневой копии (реально загруженная)
        };

        std::unordered_map<ModuleType, ModuleData>           m_Modules;
        std::unordered_map<ModuleType, ModuleReloadCallbacks> m_Callbacks;
        FileWatcher                                           m_Watcher;

        // ─── Горячая перезагрузка одного модуля ──────────────────────────────
        void ReloadModule(ModuleType type)
        {
            auto it = m_Modules.find(type);
            if (it == m_Modules.end()) return;

            std::string path       = it->second.path;       // оригинал
            std::string oldShadow  = it->second.shadowPath; // старая копия

            CHE_CORE_INFO("ModuleManager: reloading module type {}...", (int)type);

            // 1. Дать возможность Application уничтожить зависимые объекты
            auto cbIt = m_Callbacks.find(type);
            if (cbIt != m_Callbacks.end() && cbIt->second.OnBeforeReload)
                cbIt->second.OnBeforeReload();

            // 2. Выгрузить старый модуль (освобождает теневую копию)
            it->second.destroy(it->second.module);
            Unload(it->second.handle);
            m_Modules.erase(it);

            // 3. Сохранить старую теневую копию до подтверждённо успешного hot-reload
            std::string backupShadow;
            if (!oldShadow.empty())
            {
                backupShadow = MakeBackupPath(path);
                std::error_code ec;
                std::filesystem::remove(backupShadow, ec);
                ec.clear();
                std::filesystem::rename(oldShadow, backupShadow, ec);
                if (ec)
                {
                    CHE_CORE_WARN("ModuleManager: failed to save backup '{}' -> '{}': {}",
                                  oldShadow, backupShadow, ec.message());
                    backupShadow.clear();
                }
                else
                {
                    CHE_CORE_INFO("ModuleManager: old DLL backup saved '{}'", backupShadow);
                }
            }

            // 4. Загрузить новый (LoadModule сделает свежую shadow copy)
            if (!LoadModule(path))
            {
                CHE_CORE_ERROR("ModuleManager: failed to reload '{}' - module unavailable", path);
                return;
            }

            // 5. Уведомить Application о новой фабрике (критерий успеха — успешный OnAfterReload)
            bool afterReloadOk = true;
            if (cbIt != m_Callbacks.end() && cbIt->second.OnAfterReload)
            {
                try
                {
                    cbIt->second.OnAfterReload(m_Modules[type].module);
                }
                catch (const std::exception& e)
                {
                    afterReloadOk = false;
                    CHE_CORE_ERROR("ModuleManager: OnAfterReload threw exception: {}", e.what());
                }
                catch (...)
                {
                    afterReloadOk = false;
                    CHE_CORE_ERROR("ModuleManager: OnAfterReload threw unknown exception");
                }
            }

            if (afterReloadOk)
            {
                // 6. Удалить backup (старую копию) только после успешного запуска
                if (!backupShadow.empty())
                {
                    std::error_code ec;
                    std::filesystem::remove(backupShadow, ec);
                    if (ec)
                    {
                        CHE_CORE_WARN("ModuleManager: failed to delete backup '{}': {}", backupShadow, ec.message());
                    }
                }

                CHE_CORE_INFO("ModuleManager: module '{}' reloaded successfully", path);
            }
            else
            {
                CHE_CORE_ERROR("ModuleManager: module '{}' reloaded, but OnAfterReload failed (backup retained)", path);
            }
        }

        // Generate fixed shadow-copy path: module.dll -> module_temp.dll
        std::string MakeShadowPath(const std::string& originalPath)
        {
            std::filesystem::path p(originalPath);
            std::string stem = p.stem().string();
            std::string ext  = p.extension().string();
            auto dir = p.parent_path();
            return (dir / (stem + "_temp" + ext)).string();
        }

        std::string MakeBackupPath(const std::string& originalPath)
        {
            std::filesystem::path p(originalPath);
            std::string stem = p.stem().string();
            std::string ext  = p.extension().string();
            auto dir = p.parent_path();

            return (dir / (stem + "_prev" + ext)).string();
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
