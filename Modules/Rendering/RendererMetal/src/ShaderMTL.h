#pragma once

#include <CheStl/String.h>
#include <Render/UniformBlocks.h>
#include <Render/Pipeline/VertexLayout.h>

#include <unordered_map>
#include <cstdint>

namespace CHModules
{
    class ShaderMTL
    {
    public:
        ShaderMTL(const CHEngine::String& slangSource,
                  const CHEngine::String& vertEntry,
                  const CHEngine::String& fragEntry,
                  const CHEngine::String& sourcePath = CHEngine::String());
        ~ShaderMTL();

		void Bind() const;
		void Unbind() const;
        bool Reload(const CHEngine::String& slangSource,
                    const CHEngine::String& vertEntry  = CHEngine::String("vertMain"),
                    const CHEngine::String& fragEntry  = CHEngine::String("fragMain"),
                    const CHEngine::String& sourcePath = CHEngine::String());

        // --- UBO ---
        void SetUniformBlock(CHEngine::EUniformBlock block, const void* data, uint32_t size);

        // --- Sampler binding (no-op для Metal, текстуры через [[texture(N)]]) ---
        void SetInt(const CHEngine::String& name, int value);

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
        bool CompileSlang(const CHEngine::String& slangSource,
                          const CHEngine::String& vertEntry,
                          const CHEngine::String& fragEntry,
                          const CHEngine::String& sourcePath);

        void* m_Library      = nullptr;  // id<MTLLibrary>
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
