#pragma once

#include <Core.h>
#include <CheStl/String.h>

#include "IModuleFactory.h"
#include "Render/Handles.h"
#include "Render/Core/RenderTypes.h"
#include "Render/Core/RenderAPI.h"
#include "Render/Pipeline/VertexLayout.h"
#include "Render/Descriptors.h"
#include "Render/IFrameGraphBackend.h"

#include <glm/mat4x4.hpp>

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
            const String& debugName = String()
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

        // Called each frame by RenderFacade::BeginFrame / EndFrame.
        // Metal: starts/ends command buffer + sets render context for ImGui.
        // OGL: no-op (state is managed implicitly).
        // BeginFrame returns false when the frame must be skipped (e.g. window
        // minimized / 0×0 framebuffer / swapchain out-of-date). When it returns
        // false the caller must NOT record a frame graph or call EndFrame.
        virtual bool BeginFrame() { return true; }
        virtual void EndFrame()   {}

        // Backend-native ID for the texture (OGL: GLuint; VK: VkImage; ...).
        // Used by ImGui::Image to display offscreen render output.
        // Returns 0 on invalid handle.
        virtual uint64_t GetTextureNativeID(TextureHandle h) = 0;

        // ── Present ───────────────────────────────────────────────────────────
        // Blit an offscreen viewport texture to the default framebuffer/swapchain
        // with a fullscreen pass. Used by the ImGui-less runtime (Player) to show
        // the rendered frame without going through ImGui::Image. Editors keep
        // using the ImGui present path. Default: no-op (backends override).
        virtual void PresentToBackbuffer(TextureHandle viewportTex,
                                         uint32_t w, uint32_t h) {}

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

        // ── API-agnostic backend traits ───────────────────────────────────────
        // Block until the GPU is idle. Must be called before destroying GPU
        // resources during shutdown or API switch. OGL/Metal: no-op. Vulkan: vkDeviceWaitIdle.
        virtual void WaitIdle() {}

        // Correction matrix applied on top of GLM's OpenGL-convention projection.
        // OGL/Metal: identity. Vulkan: Y-flip + Z [-1,1] → [0,1].
        virtual glm::mat4 GetClipSpaceCorrection() const { return glm::mat4(1.0f); }

        // NDC depth-range constants for shaders that linearise depth.
        // Defaults: OpenGL/Metal convention (Z in [-1,1] → remap with 0.5*z+0.5).
        struct NDCDepthRange { float NearZ = -1.0f; float Scale = 0.5f; float Bias = 0.5f; };
        virtual NDCDepthRange GetNDCDepthRange() const { return {}; }

        // True if any backend texture displayed through ImGui::Image must have its
        // V coordinate flipped. OGL stores rows bottom-up (UV.y=0 = bottom);
        // Vulkan inherits the same convention for the viewport HDR→LDR path.
        // Metal stores textures top-down → no flip.
        virtual bool ImGuiImageNeedsVFlip() const { return false; }

        // True if the swapchain uses a perceptual sRGB format and the GPU applies
        // linear→sRGB on write. In that case manual gamma correction in tonemap
        // pass must be skipped to avoid double correction.
        virtual bool SwapchainIsSRGB() const { return false; }

        // Returns an opaque pointer to a backend-specific shared context struct
        // (e.g. Vulkan: RendererVK::VulkanSharedContext). Other APIs: nullptr.
        // Used by API-specific consumers (ImGuiVK) that need native handles.
        virtual void* GetNativeSharedContext() { return nullptr; }
    };
}

DECLARE_MODULE_FACTORY()
