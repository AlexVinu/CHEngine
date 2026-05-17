#pragma once

#include "Render/IRenderFactory.h"
#include "Memory/HandlePool.h"
#include <memory>

#include "UnifiedBufferMTL.h"
#include "TextureMTLFull.h"
#include "ShaderMTL.h"
#include "PipelineMTL.h"

// Legacy objects still used by EditorViewport old path
#include "BufferMTL.h"       // VertexBufferMTL + IndexBufferMTL
#include "VertexArrayMTL.h"
#include "RenderApiMTL.h"

#include <SlangBackend/SlangBackend.h>

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
            const String& debugName = String("buffer")) override;

        CHEngine::ShaderHandle CreateShader(
            const String& slangSource,
            const String& vertEntry  = String("vertMain"),
            const String& fragEntry  = String("fragMain"),
            const String& sourcePath = String()) override;

        CHEngine::TextureHandle CreateTexture(
            const uint8_t* data,
            uint32_t width, uint32_t height, uint32_t channels,
            uint32_t mipLevels, uint32_t arrayLayers,
            CHEngine::TextureFormat format,
            CHEngine::TextureType   type,
            CHEngine::TextureUsage  usage,
            CHEngine::MemoryType    memory,
            const String& debugName = String()) override;

        void Delete(CHEngine::BufferHandle   h) override;
        void Delete(CHEngine::ShaderHandle   h) override;
        void Delete(CHEngine::TextureHandle  h) override;
        void Delete(CHEngine::PipelineHandle h) override;

        void UpdateBuffer(CHEngine::BufferHandle h,
                          std::span<const std::byte> data,
                          uint64_t offset = 0) override;

        uint64_t GetTextureNativeID(CHEngine::TextureHandle h) override;

        bool ReloadShader(CHEngine::ShaderHandle h,
                          const String& slangSource,
                          const String& vertEntry,
                          const String& fragEntry,
                          const String& sourcePath) override;

        CHEngine::PipelineHandle CreatePipeline(CHEngine::PipelineDesc desc) override;

        std::unique_ptr<CHEngine::IFrameGraphBackend> CreateFrameGraphBackend() override;

        uint32_t GetUniformBufferOffsetAlignment() const override { return 256u; }

        void Init(const CHEngine::RendererInitInfo& init) override;
        void Shutdown() override;
        void BeginFrame() override;
        void EndFrame()   override;

        CHEngine::ERenderAPI GetRenderApi() override { return CHEngine::ERenderAPI::METAL; }
        CHEngine::ModuleType GetType() const override { return CHEngine::ModuleType::Render; }
        bool CheckIsWorking() override { return true; }

        // ── Legacy raw-pointer API (old EditorViewport path) ──────────────────
        class VertexBufferMTL* CreateVertexBuffer(float* vertices, uint32_t size);
        class IndexBufferMTL*  CreateIndexBuffer(uint32_t* indices, uint32_t count);
        class VertexArrayMTL*  CreateVertexArray();
        class RenderApiMTL*    CreateRenderAPI();
        void Delete(class VertexBufferMTL* ptr);
        void Delete(class IndexBufferMTL*  ptr);
        void Delete(class VertexArrayMTL*  ptr);
        void Delete(class RenderApiMTL*    ptr);

        // Slang shader compiler — one instance per factory, initialised in Init().
        CHModules::SlangBackend Slang;

    private:
        template<typename T, typename... Args>
        T* CreateImpl(Args&&... args) { return new T(std::forward<Args>(args)...); }
        template<typename T>
        void DestroyImpl(T* ptr) { delete ptr; }

        // Manages Metal context lifecycle (device, command queue, framebuffer)
        std::unique_ptr<RenderApiMTL> m_RenderApi;
    };
}
