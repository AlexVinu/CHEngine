#pragma once
#include <Core.h>

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
                Unload(handle);
                return false;
            }

            IModuleFactory* module = create();
            if (!module) {
                CHE_CORE_ERROR("LoadModule '{}': CreateFactory() returned null", path);
                Unload(handle);
                return false;
            }
            ModuleType type = module->GetType();

            m_Modules[type] = { handle, module, destroy, path };

            return true;
        }

        void UnloadAll()
        {
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
            std::string     path;
        };

        std::unordered_map<ModuleType, ModuleData>           m_Modules;

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
