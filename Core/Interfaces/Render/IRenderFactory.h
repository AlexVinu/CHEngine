#pragma once

#include <Core.h>
#include <CheStl/String.h>

#include "Memory/HandlePool.h"
#include "IModuleFactory.h"
#include "Render/Handles.h"
#include "Render/Core/RenderTypes.h"
#include "Render/Core/RenderAPI.h"
#include "Render/Pipeline/VertexLayout.h"
#include "Render/Descriptors.h"
#include "Render/IFrameGraphBackend.h"

#include <span>
#include <memory>

namespace CHEngine {

    struct IRenderFactory : IModuleFactory
    {
        virtual ~IRenderFactory() = default;

        virtual BufferHandle CreateBuffer(uint64_t size,
            BufferUsage usage,
            MemoryType memory,
            std::span<const std::byte> initialData,
            const String& debugName = String("buffer")) = 0;

        virtual ShaderHandle CreateShader(const String& slangSource,
            const String& vertEntry = String("vertMain"),
            const String& fragEntry = String("fragMain"),
            const String& sourcePath = String()) = 0;

        virtual TextureHandle CreateTexture(
            const uint8_t* data,
            uint32_t width,
            uint32_t height,
            uint32_t channels,
            uint32_t mipLevels,
            uint32_t arrayLayers,
            TextureFormat format,
            TextureType type,
            TextureUsage usage,
            MemoryType memory,
            const char* debugName
        ) = 0;

        virtual void Delete(BufferHandle  ptr) = 0;
        virtual void Delete(ShaderHandle  ptr) = 0;
        virtual void Delete(TextureHandle ptr) = 0;

        virtual ERenderAPI GetRenderApi() = 0;

        virtual bool CheckIsWorking() = 0;

        // ── API lifecycle ────────────────────────────────────────────────────
        // Init: load loader (GLAD for OGL), allocate per-API state.
        // Must be called once after the window context is created.
        virtual void Init(const RendererInitInfo& init) = 0;
        virtual void Shutdown() = 0;

        // Backend-native ID for the texture (OGL: GLuint; VK: VkImage; ...).
        // Used by ImGui::Image to display offscreen render output.
        // Returns 0 on invalid handle.
        virtual uint64_t GetTextureNativeID(TextureHandle h) = 0;

        // ── Shader hot-reload ────────────────────────────────────────────────
        // Recompiles the shader in-place. The handle stays valid.
        // Returns false if compilation/link fails (existing program is preserved).
        virtual bool ReloadShader(ShaderHandle h,
                                  const String& slangSource,
                                  const String& vertEntry,
                                  const String& fragEntry,
                                  const String& sourcePath) = 0;

        // ── Buffer update ────────────────────────────────────────────────────
        // Upload CPU data into an existing buffer (CpuToGpu only).
        // offset=0, size=data.size() for a full update.
        virtual void UpdateBuffer(BufferHandle h,
                                  std::span<const std::byte> data,
                                  uint64_t offset = 0) = 0;

        // ── Pipeline ─────────────────────────────────────────────────────────
        virtual PipelineHandle CreatePipeline(PipelineDesc desc) = 0;
        virtual void Delete(PipelineHandle h) = 0;

        // ── Frame-graph backend ───────────────────────────────────────────────
        virtual std::unique_ptr<IFrameGraphBackend> CreateFrameGraphBackend() = 0;

        // ── Alignment query ───────────────────────────────────────────────────
        // Returns the required alignment (in bytes) for UBO sub-range offsets.
        // OGL: GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT (typically 256).
        virtual uint32_t GetUniformBufferOffsetAlignment() const = 0;
    };
}

DECLARE_MODULE_FACTORY()
