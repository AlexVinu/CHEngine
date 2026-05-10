#pragma once

#include <glad/glad.h>
#include <algorithm>
#include "Render/Core/RenderAPI.h"

namespace CHModules {

class FramebufferOGL {
public:
    FramebufferOGL(uint32_t width, uint32_t height,
                   CHEngine::FramebufferColorFormat colorFormat = CHEngine::FramebufferColorFormat::RGBA8);
    ~FramebufferOGL();

    void Bind()   const ;
    void Unbind() const ;
    void Resize(uint32_t width, uint32_t height) ;

    uint32_t GetColorAttachmentID() const  { return m_ColorAttachment; }
    uint32_t GetWidth()  const  { return m_Width; }
    uint32_t GetHeight() const  { return m_Height; }

private:
    void CreateAttachments();
    void DeleteAttachments();

    GLuint   m_RendererID      = 0;
    GLuint   m_ColorAttachment = 0;
    GLuint   m_DepthAttachment = 0;
    uint32_t m_Width  = 0;
    uint32_t m_Height = 0;
    CHEngine::FramebufferColorFormat m_ColorFormat = CHEngine::FramebufferColorFormat::RGBA8;
};

} // namespace CHModules
