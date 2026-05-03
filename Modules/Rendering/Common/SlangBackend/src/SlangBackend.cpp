#include "SlangBackend/SlangBackend.h"

#include <slang.h>
#include <slang-com-ptr.h>

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace CHModules
{
    namespace
    {
        struct TargetMapping
        {
            SlangCompileTarget target;
            const char*        profile;
            bool               supported;
        };

        TargetMapping ResolveTarget(CHEngine::ERenderAPI api)
        {
            switch (api)
            {
                case CHEngine::ERenderAPI::OPENGL:
                    return { SLANG_GLSL,   "glsl_410", true };
                case CHEngine::ERenderAPI::VULKAN:
                    return { SLANG_SPIRV,  "spirv_1_5", true };
                case CHEngine::ERenderAPI::METAL:
                    return { SLANG_METAL,  "metal_3_2", true };
                case CHEngine::ERenderAPI::DIRECTX12:
                    return { SLANG_HLSL,   "sm_6_0", true };
                case CHEngine::ERenderAPI::DIRECTX11:
                    return { SLANG_HLSL,   "sm_5_0", true };
                case CHEngine::ERenderAPI::NONE:
                default:
                    return { SLANG_TARGET_UNKNOWN, nullptr, false };
            }
        }

        class SlangBackendImpl final : public SlangBackend
        {
        public:
            bool Init(CHEngine::ERenderAPI api)
            {
                const TargetMapping mapping = ResolveTarget(api);
                if (!mapping.supported)
                    return false;

                if (SLANG_FAILED(slang::createGlobalSession(m_GlobalSession.writeRef())))
                    return false;

                m_Api = api;
                return true;
            }

            CompiledShader Compile(
                const CHEngine::String& source,
                const CHEngine::String& vertEntry,
                const CHEngine::String& fragEntry,
                const CHEngine::String& sourcePath) override
            {
                CompiledShader result;

                // Create a fresh session for every compile so that `import common`
                // (and any other imported modules) are always re-read from disk.
                // Reusing a session causes Slang to serve cached modules whose
                // GLSL name-mangling differs between vert/frag stages, producing
                // "struct fields mismatch" link errors for shared UBO types.
                const TargetMapping mapping = ResolveTarget(m_Api);

                // CRITICAL: matrices are uploaded as column-major (std140 / glm
                // default). Force column-major on the target so Slang emits
                // "layout(column_major) uniform;" in GLSL and keeps mul(M, v)
                // correct. defaultMatrixLayoutMode on SessionDesc alone does not
                // affect the GLSL pragma — we must use the CompilerOptionEntry.
                slang::CompilerOptionEntry matrixLayoutOpt{};
                matrixLayoutOpt.name                  = slang::CompilerOptionName::MatrixLayoutColumn;
                matrixLayoutOpt.value.kind            = slang::CompilerOptionValueKind::Int;
                matrixLayoutOpt.value.intValue0       = 1; // true

                slang::TargetDesc targetDesc{};
                targetDesc.format                    = mapping.target;
                targetDesc.profile                   = m_GlobalSession->findProfile(mapping.profile);
                targetDesc.compilerOptionEntries     = &matrixLayoutOpt;
                targetDesc.compilerOptionEntryCount  = 1;

                slang::SessionDesc sessionDesc{};
                sessionDesc.targets     = &targetDesc;
                sessionDesc.targetCount = 1;
                sessionDesc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;

                Slang::ComPtr<slang::ISession> session;
                if (SLANG_FAILED(m_GlobalSession->createSession(sessionDesc, session.writeRef())))
                {
                    result.errorLog = "Slang: failed to create session";
                    return result;
                }

                // Slang uses the path argument to resolve `import` directives —
                // it takes the directory from this path as the search base.
                const char* pathArg = sourcePath.empty() ? "shader.slang" : sourcePath.c_str();

                Slang::ComPtr<slang::IBlob> diagBlob;
                Slang::ComPtr<slang::IModule> module(
                    session->loadModuleFromSourceString(
                        "shader", pathArg, source.c_str(), diagBlob.writeRef()));

                if (!module)
                {
                    result.errorLog = diagBlob
                        ? static_cast<const char*>(diagBlob->getBufferPointer())
                        : "Slang: unknown compile error";
                    return result;
                }

                Slang::ComPtr<slang::IEntryPoint> vert, frag;
                module->findEntryPointByName(vertEntry.c_str(), vert.writeRef());
                module->findEntryPointByName(fragEntry.c_str(), frag.writeRef());

                if (!vert || !frag)
                {
                    result.errorLog = "Slang: entry points not found: ";
                    result.errorLog += vertEntry.c_str();
                    result.errorLog += ", ";
                    result.errorLog += fragEntry.c_str();
                    return result;
                }

                slang::IComponentType* parts[] = { module, vert, frag };
                Slang::ComPtr<slang::IComponentType> program, linked;

                if (SLANG_FAILED(session->createCompositeComponentType(parts, 3, program.writeRef(), nullptr)))
                {
                    result.errorLog = "Slang: failed to compose program";
                    return result;
                }

                Slang::ComPtr<slang::IBlob> linkDiag;
                if (SLANG_FAILED(program->link(linked.writeRef(), linkDiag.writeRef())))
                {
                    result.errorLog = linkDiag
                        ? static_cast<const char*>(linkDiag->getBufferPointer())
                        : "Slang: link failed";
                    return result;
                }

                ReflectParams(linked->getLayout(), result);

                if (m_Api == CHEngine::ERenderAPI::METAL)
                {
                    // Metal MSL: both stages live in one library — use getTargetCode
                    // so the output contains vertex + fragment functions together.
                    // Store in vertexCode; ShaderMTL only reads that field.
                    Slang::ComPtr<slang::IBlob> blob, diag;
                    const SlangResult hr = linked->getTargetCode(0, blob.writeRef(), diag.writeRef());
                    if (SLANG_SUCCEEDED(hr) && blob)
                    {
                        const uint8_t* p = static_cast<const uint8_t*>(blob->getBufferPointer());
                        result.vertexCode.assign(p, p + blob->getBufferSize());
                        result.fragmentCode = result.vertexCode; // same source
                    }
                    else
                    {
                        result.errorLog = diag
                            ? static_cast<const char*>(diag->getBufferPointer())
                            : "Slang Metal: getTargetCode failed";
                    }
                }
                else
                {
                    ExtractEntryPoint(linked, 0, result.vertexCode, result.errorLog);
                    ExtractEntryPoint(linked, 1, result.fragmentCode, result.errorLog);
                }

                result.valid = !result.vertexCode.empty() && !result.fragmentCode.empty();
                return result;
            }

        private:
            static void ExtractEntryPoint(
                slang::IComponentType* linked,
                int                    entryIdx,
                std::vector<uint8_t>&  out,
                std::string&           errorLog)
            {
                Slang::ComPtr<slang::IBlob> blob, diag;
                const SlangResult hr = linked->getEntryPointCode(entryIdx, 0, blob.writeRef(), diag.writeRef());
                if (SLANG_SUCCEEDED(hr) && blob)
                {
                    const uint8_t* p = static_cast<const uint8_t*>(blob->getBufferPointer());
                    out.assign(p, p + blob->getBufferSize());
                }
                else if (diag)
                {
                    errorLog += static_cast<const char*>(diag->getBufferPointer());
                }
            }

            static void ReflectParams(slang::ProgramLayout* layout, CompiledShader& out)
            {
                if (!layout)
                    return;

                const uint32_t count = static_cast<uint32_t>(layout->getParameterCount());
                for (uint32_t i = 0; i < count; ++i)
                {
                    slang::VariableLayoutReflection* var = layout->getParameterByIndex(i);
                    if (!var)
                        continue;

                    ShaderParamInfo info;
                    info.binding = static_cast<uint32_t>(var->getBindingIndex());
                    info.set     = static_cast<uint32_t>(var->getBindingSpace());

                    slang::TypeLayoutReflection* typeLayout = var->getTypeLayout();
                    if (typeLayout)
                    {
                        info.size = static_cast<uint32_t>(typeLayout->getSize());
                        const auto kind = typeLayout->getKind();
                        info.isSampler =
                            kind == slang::TypeReflection::Kind::Resource ||
                            kind == slang::TypeReflection::Kind::SamplerState;
                    }

                    if (const char* name = var->getName())
                        out.params[name] = info;
                }
            }

            Slang::ComPtr<slang::IGlobalSession> m_GlobalSession;
            CHEngine::ERenderAPI                 m_Api = CHEngine::ERenderAPI::NONE;
        };

        struct Registry
        {
            std::mutex mutex;
            std::unordered_map<CHEngine::ERenderAPI, std::unique_ptr<SlangBackendImpl>> backends;
        };

        Registry& GetRegistry()
        {
            static Registry registry;
            return registry;
        }
    }

    SlangBackend* SlangBackend::GetForApi(CHEngine::ERenderAPI api)
    {
        if (!ResolveTarget(api).supported)
            return nullptr;

        auto& registry = GetRegistry();
        std::lock_guard<std::mutex> lock(registry.mutex);

        auto it = registry.backends.find(api);
        if (it != registry.backends.end())
            return it->second.get();

        auto backend = std::make_unique<SlangBackendImpl>();
        if (!backend->Init(api))
            return nullptr;

        SlangBackendImpl* raw = backend.get();
        registry.backends.emplace(api, std::move(backend));
        return raw;
    }

    void SlangBackend::Shutdown()
    {
        auto& registry = GetRegistry();
        std::lock_guard<std::mutex> lock(registry.mutex);
        registry.backends.clear();
    }
}
