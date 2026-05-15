#pragma once

#include "Render/IRenderFactory.h"
#include "Memory/HandlePool.h"

#include "BufferOGL.h"
#include "PipelineOGL.h"
#include "ShaderOGL.h"
#include "TextureOGL.h"
#include "RenderApiOGL.h"
#include "IsRenderAvailable.h"

#include <unordered_map>
#include <vector>
#include <memory>

namespace CHModules
{
    using ShaderPool   = CHEngine::HandlePool<ShaderOGL,   CHEngine::ShaderTag>;
    using TexturePool  = CHEngine::HandlePool<TextureOGL,  CHEngine::TextureTag>;
    using BufferPool   = CHEngine::HandlePool<BufferOGL,   CHEngine::BufferTag>;
    using PipelinePool = CHEngine::HandlePool<PipelineOGL, CHEngine::PipelineTag>;

    struct RenderFactoryOGL : public CHEngine::IRenderFactory
    {
        // ── Resource creation ─────────────────────────────────────────────────
        virtual CHEngine::BufferHandle CreateBuffer(uint64_t size,
            CHEngine::BufferUsage usage,
            CHEngine::MemoryType memory,
            std::span<const std::byte> initialData,
            const String& debugName = String("buffer")) override;

        virtual CHEngine::ShaderHandle CreateShader(const String& slangSource,
            const String& vertEntry = String("vertMain"),
            const String& fragEntry = String("fragMain"),
            const String& sourcePath = String()) override;

        virtual CHEngine::TextureHandle CreateTexture(
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
            const String& debugName = String()
        ) override;

        virtual void Delete(CHEngine::BufferHandle  handle) override;
        virtual void Delete(CHEngine::ShaderHandle  handle) override;
        virtual void Delete(CHEngine::TextureHandle handle) override;

        // ── Buffer update ──────────────────────────────────────────────────────
        virtual void UpdateBuffer(CHEngine::BufferHandle h,
                                  std::span<const std::byte> data,
                                  uint64_t offset = 0) override;

        // ── Texture native ID (for ImGui::Image) ──────────────────────────────
        virtual uint64_t GetTextureNativeID(CHEngine::TextureHandle h) override;

        // ── Shader hot-reload ──────────────────────────────────────────────────
        virtual bool ReloadShader(CHEngine::ShaderHandle h,
                                  const String& slangSource,
                                  const String& vertEntry,
                                  const String& fragEntry,
                                  const String& sourcePath) override;

        // ── Pipeline ──────────────────────────────────────────────────────────
        virtual CHEngine::PipelineHandle CreatePipeline(CHEngine::PipelineDesc desc) override;
        virtual void Delete(CHEngine::PipelineHandle handle) override;

        // ── Frame-graph backend ────────────────────────────────────────────────
        virtual std::unique_ptr<CHEngine::IFrameGraphBackend> CreateFrameGraphBackend() override;

        virtual CHEngine::ERenderAPI GetRenderApi() override { return CHEngine::ERenderAPI::OPENGL; }
        virtual CHEngine::ModuleType GetType() const override { return CHEngine::ModuleType::Render; }

        virtual bool CheckIsWorking() override;

        // ── API lifecycle ──────────────────────────────────────────────────────
        virtual void Init(const CHEngine::RendererInitInfo& init) override;
        virtual void Shutdown() override;

        // ── Alignment query ────────────────────────────────────────────────────
        virtual uint32_t GetUniformBufferOffsetAlignment() const override { return m_UBOOffsetAlignment; }

        // ── Resource pools ─────────────────────────────────────────────────────
        ShaderPool   Shaders;
        TexturePool  Textures;
        BufferPool   Buffers;
        PipelinePool Pipelines;

        uint32_t m_UBOOffsetAlignment = 256; // cached GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT

        // Backend-internal API state (GLAD loader + draw helpers).
        RendererApiOGL Api;

    private:
        // ShaderHandle index → list of dependent PipelineHandles (for hot-reload).
        std::unordered_map<uint32_t, std::vector<CHEngine::PipelineHandle>> m_ShaderToPipelines;
    };
}
