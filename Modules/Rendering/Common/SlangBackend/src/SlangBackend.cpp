#include "SlangBackend/SlangBackend.h"

#include <slang.h>
#include <slang-com-ptr.h>

#include <string>

namespace CHModules
{
    // ── Implementation detail ─────────────────────────────────────────────────
    // Defined here so that slang.h stays out of the public header.

    struct SlangBackendImpl
    {
        Slang::ComPtr<slang::IGlobalSession> GlobalSession;
        CHEngine::ERenderAPI                 Api = CHEngine::ERenderAPI::NONE;
    };

    // ── Internal helpers (file-scope, unchanged from old code) ────────────────

    namespace {

        struct TargetMapping
        {
            SlangCompileTarget target;
            const char* profile;
            bool               supported;
        };

        static void ExtractEntryPoint(
            slang::IComponentType* linked,
            int                    entryIdx,
            std::vector<uint8_t>& out,
            std::string& errorLog)
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
                info.set = static_cast<uint32_t>(var->getBindingSpace());

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

    } // anonymous namespace

    TargetMapping ResolveTarget(CHEngine::ERenderAPI api)
    {
        switch (api)
        {
            case CHEngine::ERenderAPI::OPENGL:
                return { SLANG_GLSL,   "glsl_410", true };  // macOS ограничен OpenGL 4.1
            case CHEngine::ERenderAPI::VULKAN:
                return { SLANG_SPIRV,  "spirv_1_5", true };
            case CHEngine::ERenderAPI::METAL:
                return { SLANG_METAL,  "metal", true };
            case CHEngine::ERenderAPI::DIRECTX12:
                return { SLANG_HLSL,   "sm_6_0", true };
            case CHEngine::ERenderAPI::DIRECTX11:
                return { SLANG_HLSL,   "sm_5_0", true };
            case CHEngine::ERenderAPI::NONE:
            default:
                return { SLANG_TARGET_UNKNOWN, nullptr, false };
        }
    }

    // ── SlangBackend public API ───────────────────────────────────────────────

    SlangBackend::SlangBackend()
        : m_Impl(std::make_unique<SlangBackendImpl>())
    {
    }

    SlangBackend::~SlangBackend() = default;

    bool SlangBackend::IsInitialised() const
    {
        return m_Impl && m_Impl->GlobalSession != nullptr;
    }

    bool SlangBackend::Init(CHEngine::ERenderAPI api)
    {
        const TargetMapping mapping = ResolveTarget(api);
        if (!mapping.supported)
            return false;

        Slang::ComPtr<slang::IGlobalSession> globalSession;
        if (SLANG_FAILED(slang::createGlobalSession(globalSession.writeRef())))
            return false;

        m_Impl->GlobalSession = std::move(globalSession);
        m_Impl->Api           = api;
        return true;
    }

    CompiledShader SlangBackend::Compile(
        const String& source,
        const String& vertEntry,
        const String& fragEntry,
        const String& sourcePath)
    {
        CompiledShader result;

        // Create a fresh session for every compile so that `import common`
        // (and any other imported modules) are always re-read from disk.
        // Reusing a session causes Slang to serve cached modules whose
        // GLSL name-mangling differs between vert/frag stages, producing
        // "struct fields mismatch" link errors for shared UBO types.
        const TargetMapping mapping = ResolveTarget(m_Impl->Api);

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
        targetDesc.profile                   = m_Impl->GlobalSession->findProfile(mapping.profile);
        targetDesc.compilerOptionEntries     = &matrixLayoutOpt;
        targetDesc.compilerOptionEntryCount  = 1;

        slang::SessionDesc sessionDesc{};
        sessionDesc.targets     = &targetDesc;
        sessionDesc.targetCount = 1;
        sessionDesc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;

        Slang::ComPtr<slang::ISession> session;
        if (SLANG_FAILED(m_Impl->GlobalSession->createSession(sessionDesc, session.writeRef())))
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

        if (m_Impl->Api == CHEngine::ERenderAPI::METAL)
        {
            // Metal MSL: both stages must live in ONE library.
            // getTargetCode() produces a single MSL blob with all entry points.
            // Store it in both vertexCode and fragmentCode so ShaderMTL sees it.
            Slang::ComPtr<slang::IBlob> blob, diag;
            const SlangResult hr = linked->getTargetCode(0, blob.writeRef(), diag.writeRef());
            if (SLANG_SUCCEEDED(hr) && blob)
            {
                const uint8_t* p = static_cast<const uint8_t*>(blob->getBufferPointer());
                result.vertexCode.assign(p, p + blob->getBufferSize());
                result.fragmentCode = result.vertexCode; // same source for both
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

} // namespace CHModules
