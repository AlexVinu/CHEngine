#pragma once
#include <Core.h>

#include <filesystem>
#include <string>
#include <unordered_map>

#include "IModuleFactory.h"

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

    class ModuleManager
    {
    public:
        ModuleManager() = default;

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

        void UnloadAll()
        {
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
            std::string     path;       // путь к оригинальной dylib
            std::string     shadowPath; // путь к теневой копии (реально загруженная)
        };

        std::unordered_map<ModuleType, ModuleData>           m_Modules;

        // Generate fixed shadow-copy path: module.dll -> module_temp.dll
        std::string MakeShadowPath(const std::string& originalPath)
        {
            std::filesystem::path p(originalPath);
            std::string stem = p.stem().string();
            std::string ext  = p.extension().string();
            auto dir = p.parent_path();
            return (dir / (stem + "_temp" + ext)).string();
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
