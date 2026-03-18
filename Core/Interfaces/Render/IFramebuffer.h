#pragma once
#include <cstdint>

namespace CHEngine {

    struct FramebufferSpec {
        uint32_t Width  = 1280;
        uint32_t Height = 720;
    };

    class IFramebuffer {
    public:
        virtual ~IFramebuffer() = default;

        virtual void Bind()   const = 0;
        virtual void Unbind() const = 0;

        // Recreate GPU resources at the new size.
        // Must be called from the render thread (OpenGL context active).
        virtual void Resize(uint32_t width, uint32_t height) = 0;

        // Returns the raw OpenGL texture ID of the color attachment.
        // Used by ImGui::Image — cast to (ImTextureID)(uintptr_t)id.
        virtual uint32_t GetColorAttachmentID() const = 0;

        // Returns native color attachment as void* for ImGui::Image.
        // OpenGL: (void*)(uintptr_t)glTextureID.
        // Metal: id<MTLTexture> cast to void*.
        virtual void* GetNativeColorAttachment() const
        {
            return (void*)(uintptr_t)GetColorAttachmentID();
        }

        virtual uint32_t GetWidth()  const = 0;
        virtual uint32_t GetHeight() const = 0;
    };

} // namespace CHEngine
