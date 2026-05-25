#pragma once

#include "Render/IRenderFactory.h"
#include "Memory/HandlePool.h"

#include "BufferVK.h"
#include "PipelineVK.h"
#include "ShaderVK.h"
#include "TextureVK.h"
#include "VulkanContext.h"

#include <SlangBackend/SlangBackend.h>
#include <vulkan/vulkan.h>
#include <memory>

namespace CHModules {

    using ShaderPoolVK   = CHEngine::HandlePool<ShaderVK,   CHEngine::ShaderTag>;
    using TexturePoolVK  = CHEngine::HandlePool<TextureVK,  CHEngine::TextureTag>;
    using BufferPoolVK   = CHEngine::HandlePool<BufferVK,   CHEngine::BufferTag>;
    using PipelinePoolVK = CHEngine::HandlePool<PipelineVK, CHEngine::PipelineTag>;

    struct RenderFactoryVK : public CHEngine::IRenderFactory
    {
        // ── API lifecycle ─────────────────────────────────────────────────────
        void Init(const CHEngine::RendererInitInfo& init) override;
        void Shutdown() override;
        void BeginFrame() override;
        void EndFrame()   override;

        // ── Resource creation ─────────────────────────────────────────────────
        CHEngine::BufferHandle CreateBuffer(
            uint64_t size,
            CHEngine::BufferUsage usage,
            CHEngine::MemoryType memory,
            std::span<const std::byte> initialData,
            const String& debugName = String("buffer")) override;

        CHEngine::ShaderHandle CreateShader(
            const String& slangSource,
            const String& vertEntry  = String("vertMain"),
            const String& fragEntry  = String("fragMain"),
            const String& sourcePath = String()) override;

        CHEngine::TextureHandle CreateTexture(
            const uint8_t* data,
            uint32_t width,
            uint32_t height,
            uint32_t channels,
            uint32_t mipLevels,
            uint32_t arrayLayers,
            CHEngine::TextureFormat format,
            CHEngine::TextureType type,
            CHEngine::TextureUsage usage,
            CHEngine::MemoryType memory,
            const String& debugName = String()) override;

        CHEngine::PipelineHandle CreatePipeline(CHEngine::PipelineDesc desc) override;

        void Delete(CHEngine::BufferHandle  h) override;
        void Delete(CHEngine::ShaderHandle  h) override;
        void Delete(CHEngine::TextureHandle h) override;
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

        std::unique_ptr<CHEngine::IFrameGraphBackend> CreateFrameGraphBackend() override;

        CHEngine::ERenderAPI GetRenderApi() override { return CHEngine::ERenderAPI::VULKAN; }
        CHEngine::ModuleType GetType() const override { return CHEngine::ModuleType::Render; }
        bool CheckIsWorking() override;
        uint32_t GetUniformBufferOffsetAlignment() const override { return m_UboAlignment; }

        // ── API-agnostic backend traits ────────────────────────────────────────
        void           WaitIdle() override;
        glm::mat4      GetClipSpaceCorrection() const override;
        NDCDepthRange  GetNDCDepthRange() const override { return { 0.0f, 1.0f, 0.0f }; }
        // negative viewport height (FrameGraphBackendVK) уже даёт top-down
        // ориентацию в LDR-таргете, аналогичную Metal → V-flip не нужен.
        bool           ImGuiImageNeedsVFlip() const override { return false; }
        bool           SwapchainIsSRGB() const override;
        void*          GetNativeSharedContext() override;

        // ── Internal accessors for PipelineVK / FrameGraphBackendVK ───────────
        VkDevice        GetDevice()  const;
        VulkanContext*  GetContext()       { return &m_Context; }

        // ── Resource pools ─────────────────────────────────────────────────────
        ShaderPoolVK   Shaders;
        TexturePoolVK  Textures;
        BufferPoolVK   Buffers;
        PipelinePoolVK Pipelines;

        CHModules::SlangBackend Slang;

    private:
        VulkanContext m_Context;
        uint32_t      m_UboAlignment = 256;
        bool          m_Initialized  = false;
    };

} // namespace CHModules
