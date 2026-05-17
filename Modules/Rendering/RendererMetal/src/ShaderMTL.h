#pragma once

#include <CheStl/String.h>
#include <Render/UniformBlocks.h>
#include <Render/Pipeline/VertexLayout.h>
#include <SlangBackend/SlangBackend.h>

#include <unordered_map>
#include <cstdint>

namespace CHModules
{
    class ShaderMTL
    {
    public:
        // slang: reference to the factory-owned SlangBackend (must outlive this shader)
        ShaderMTL(const String& slangSource,
                  const String& vertEntry,
                  const String& fragEntry,
                  const String& sourcePath,
                  SlangBackend& slang);
        ~ShaderMTL();

		void Bind() const;
		void Unbind() const;
        bool Reload(const String& slangSource,
                    const String& vertEntry  = String("vertMain"),
                    const String& fragEntry  = String("fragMain"),
                    const String& sourcePath = String());

        // --- UBO ---
        void SetUniformBlock(CHEngine::EUniformBlock block, const void* data, uint32_t size);

        // --- Sampler binding (no-op для Metal, текстуры через [[texture(N)]]) ---
        void SetInt(const String& name, int value);

        // Получить/создать pipeline state для данного vertex layout
        void* GetOrCreatePipelineState(const CHEngine::VertexInputLayout& layout,
                                       uint32_t colorPixelFormat,
                                       uint32_t depthPixelFormat,
                                       bool blendEnabled);

        // Загрузить UBO данные в encoder
        void FlushUniforms(void* encoder) const;

        // Access compiled Metal functions (for PipelineMTL)
        void* GetVertexFunc()   const { return m_VertexFunc; }
        void* GetFragmentFunc() const { return m_FragmentFunc; }

    private:
        bool CompileSlang(const String& slangSource,
                          const String& vertEntry,
                          const String& fragEntry,
                          const String& sourcePath);

        SlangBackend* m_Slang  = nullptr; // owned by factory; valid for shader lifetime
        void* m_Library        = nullptr;  // id<MTLLibrary>
        void* m_VertexFunc   = nullptr;  // id<MTLFunction>
        void* m_FragmentFunc = nullptr;  // id<MTLFunction>

        // Pipeline state cache
        struct PipelineCacheKey {
            uint32_t stride;
            uint32_t numAttribs;
            uint32_t colorFormat;
            uint32_t depthFormat;
            bool blend;
            bool operator==(const PipelineCacheKey& o) const {
                return stride == o.stride && numAttribs == o.numAttribs &&
                       colorFormat == o.colorFormat && depthFormat == o.depthFormat &&
                       blend == o.blend;
            }
        };
        struct PipelineHash {
            size_t operator()(const PipelineCacheKey& k) const {
                size_t h = std::hash<uint32_t>()(k.stride);
                h ^= std::hash<uint32_t>()(k.numAttribs) << 1;
                h ^= std::hash<uint32_t>()(k.colorFormat) << 2;
                h ^= std::hash<uint32_t>()(k.depthFormat) << 3;
                h ^= std::hash<bool>()(k.blend) << 4;
                return h;
            }
        };
        std::unordered_map<PipelineCacheKey, void*, PipelineHash> m_PipelineCache;

        // UBO данные (std140-совместимые структуры)
        CHEngine::UBOCamera    m_Camera;
        CHEngine::UBOObject    m_Object;
        CHEngine::UBOLighting  m_Lighting;
        CHEngine::UBOMaterial  m_Material;
    };
}
