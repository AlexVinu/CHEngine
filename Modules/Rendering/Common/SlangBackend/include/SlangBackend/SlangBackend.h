#pragma once

#include <Core.h>
#include <CheStl/String.h>
#include <RenderData.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(CHE_PLATFORM_WINDOWS)
    #if defined(CHE_BUILD_SLANG_BACKEND_DLL)
        #define CHE_SLANG_API __declspec(dllexport)
    #else
        #define CHE_SLANG_API __declspec(dllimport)
    #endif
#else
    #define CHE_SLANG_API __attribute__((visibility("default")))
#endif

namespace CHModules
{
    struct ShaderParamInfo
    {
        uint32_t binding   = 0;
        uint32_t set       = 0;
        uint32_t size      = 0;
        bool     isSampler = false;
    };

    struct CompiledShader
    {
        std::vector<uint8_t> vertexCode;
        std::vector<uint8_t> fragmentCode;
        std::unordered_map<std::string, ShaderParamInfo> params;
        std::string errorLog;
        bool valid = false;
    };

    class CHE_SLANG_API SlangBackend
    {
    public:
        virtual ~SlangBackend() = default;

        bool Init(CHEngine::ERenderAPI api);

        // sourcePath: optional full path of the source file. When provided,
        // Slang uses its directory to resolve `import` directives.
        virtual CompiledShader Compile(
            const String& source,
            const String& vertEntry  = String("vertMain"),
            const String& fragEntry  = String("fragMain"),
            const String& sourcePath = String());

        static SlangBackend* GetForApi(CHEngine::ERenderAPI api);
        static void Shutdown();
    };
}
