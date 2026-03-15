#pragma once
#include <Core.h>

#include <utility>

#include "IModuleFactory.h"

// Platform-specific dynamic library includes (must be outside namespace)
#ifdef CHE_PLATFORM_WINDOWS
    #include <Windows.h>
#elif defined(CHE_PLATFORM_LINUX) || defined(CHE_PLATFORM_APPLE)
    #include <dlfcn.h>
#endif

namespace CHEngine
{
	using DestroyFn = void(*)(IModuleFactory*);
    using CreateFn = IModuleFactory* (*)();

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

        ~ModuleManager() { UnloadAll(); }

        bool LoadModule(const std::string& path)
        {
            ModuleHandle handle = load(path.c_str());
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
                getSymbol(handle, "CreateFactory"));

            auto destroy = reinterpret_cast<DestroyFn>(
                getSymbol(handle, "DestroyFactory"));

            if (!create || !destroy)
            {
                CHE_CORE_ERROR("LoadModule '{}': CreateFactory/DestroyFactory symbol not found", path);
                return false;
            }

            IModuleFactory* module = create();

            m_Modules[module->GetType()] =
            {
                handle,
                module,
                destroy
            };

            return true;
        }

        void UnloadAll()
        {
            for (auto& [type, data] : m_Modules)
            {
                data.destroy(data.module);
                unload(data.handle);
            }
            m_Modules.clear();
        }

        template<typename T>
        T* GetModule(ModuleType type)
        {
            auto it = m_Modules.find(type);
            if (it == m_Modules.end())
            {
                CHE_CORE_ERROR("Module Manager did not find type ({0} in  integer)", (int)type);
                return nullptr;
            }

            return static_cast<T*>(it->second.module);
        }

    private:
        struct ModuleData
        {
            ModuleHandle handle;
            IModuleFactory* module;
            DestroyFn destroy;
        };

        std::unordered_map<ModuleType, ModuleData> m_Modules;

        ModuleHandle load(const char* path)
        {
        #if defined(CHE_PLATFORM_WINDOWS)
            return LoadLibraryA(path);
        #else
            return dlopen(path, RTLD_NOW);
        #endif
        }

        void* getSymbol(ModuleHandle handle, const char* name)
        {
        #if defined(CHE_PLATFORM_WINDOWS)
            return reinterpret_cast<void*>(
                GetProcAddress(handle, name));
        #else
            return dlsym(handle, name);
        #endif
        }

        void unload(ModuleHandle handle)
        {
        #if defined(CHE_PLATFORM_WINDOWS)
            FreeLibrary(handle);
        #else
            dlclose(handle);
        #endif
            handle = nullptr;
        }
	};
}