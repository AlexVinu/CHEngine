#pragma once

#include "Render/IRenderFactory.h"
#include "Memory/HandlePool.h"

#include "UnifiedBufferMTL.h"
#include "TextureMTLFull.h"
#include "ShaderMTL.h"
#include "PipelineMTL.h"

// Legacy objects still used by EditorViewport old path
#include "BufferMTL.h"       // VertexBufferMTL + IndexBufferMTL
#include "VertexArrayMTL.h"
#include "RenderApiMTL.h"

namespace CHModules
{
    using BufferPoolMTL   = CHEngine::HandlePool<UnifiedBufferMTL, CHEngine::BufferTag>;
    using TexturePoolMTL  = CHEngine::HandlePool<TextureMTLFull,   CHEngine::TextureTag>;
    using ShaderPoolMTL   = CHEngine::HandlePool<ShaderMTL,        CHEngine::ShaderTag>;
    using PipelinePoolMTL = CHEngine::HandlePool<PipelineMTL,      CHEngine::PipelineTag>;

    struct RenderFactoryMTL : public CHEngine::IRenderFactory
    {
        // ── Handle-based resource pools ───────────────────────────────────────
        BufferPoolMTL   Buffers;
        TexturePoolMTL  Textures;
        ShaderPoolMTL   Shaders;
        PipelinePoolMTL Pipelines;

        // ── New handle-based API (IRenderFactory) ─────────────────────────────
        CHEngine::BufferHandle CreateBuffer(uint64_t size,
            CHEngine::BufferUsage usage,
            CHEngine::MemoryType  memory,
            std::span<const std::byte> initialData,
            const CHEngine::String& debugName = CHEngine::String("buffer")) override;

        CHEngine::ShaderHandle CreateShader(
            const CHEngine::String& slangSource,
            const CHEngine::String& vertEntry  = CHEngine::String("vertMain"),
            const CHEngine::String& fragEntry  = CHEngine::String("fragMain"),
            const CHEngine::String& sourcePath = CHEngine::String()) override;

        CHEngine::TextureHandle CreateTexture(
            const uint8_t* data,
            uint32_t width, uint32_t height, uint32_t channels,
            uint32_t mipLevels, uint32_t arrayLayers,
            CHEngine::TextureFormat format,
            CHEngine::TextureType   type,
            CHEngine::TextureUsage  usage,
            CHEngine::MemoryType    memory,
            const CHEngine::String& debugName = CHEngine::String()) override;

        void Delete(CHEngine::BufferHandle   h) override;
        void Delete(CHEngine::ShaderHandle   h) override;
        void Delete(CHEngine::TextureHandle  h) override;
        void Delete(CHEngine::PipelineHandle h) override;

        void UpdateBuffer(CHEngine::BufferHandle h,
                          std::span<const std::byte> data,
                          uint64_t offset = 0) override;

        uint64_t GetTextureNativeID(CHEngine::TextureHandle h) override;

        bool ReloadShader(CHEngine::ShaderHandle h,
                          const CHEngine::String& slangSource,
                          const CHEngine::String& vertEntry,
                          const CHEngine::String& fragEntry,
                          const CHEngine::String& sourcePath) override;

        CHEngine::PipelineHandle CreatePipeline(CHEngine::PipelineDesc desc) override;

        std::unique_ptr<CHEngine::IFrameGraphBackend> CreateFrameGraphBackend() override;

        uint32_t GetUniformBufferOffsetAlignment() const override { return 256u; }

        void Init(const CHEngine::RendererInitInfo& /*init*/) override {}
        void Shutdown() override {}

        CHEngine::ERenderAPI GetRenderApi() override { return CHEngine::ERenderAPI::METAL; }
        CHEngine::ModuleType GetType() const override { return CHEngine::ModuleType::Render; }
        bool CheckIsWorking() override { return true; }

        // ── Legacy raw-pointer API (old EditorViewport path) ──────────────────
        CHEngine::IVertexBuffer* CreateVertexBuffer(float* vertices, uint32_t size);
        CHEngine::IIndexBuffer*  CreateIndexBuffer(uint32_t* indices, uint32_t count);
        CHEngine::IVertexArray*  CreateVertexArray();
        CHEngine::IRenderApi*    CreateRenderAPI();
        void Delete(CHEngine::IVertexBuffer* ptr);
        void Delete(CHEngine::IIndexBuffer*  ptr);
        void Delete(CHEngine::IVertexArray*  ptr);
        void Delete(CHEngine::IRenderApi*    ptr);

    private:
        template<typename T, typename... Args>
        T* CreateImpl(Args&&... args) { return new T(std::forward<Args>(args)...); }
        template<typename T>
        void DestroyImpl(T* ptr) { delete ptr; }
    };
}
