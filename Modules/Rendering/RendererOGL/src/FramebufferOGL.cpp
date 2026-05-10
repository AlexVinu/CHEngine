#include "FramebufferOGL.h"
#include <Log/Log.h>
#include <algorithm>

namespace CHModules {

FramebufferOGL::FramebufferOGL(uint32_t width, uint32_t height,
                               CHEngine::FramebufferColorFormat colorFormat)
    : m_Width(std::max(width, 1u))
    , m_Height(std::max(height, 1u))
    , m_ColorFormat(colorFormat)
{
    glGenFramebuffers(1, &m_RendererID);
    CreateAttachments();
}

FramebufferOGL::~FramebufferOGL()
{
    DeleteAttachments();
    glDeleteFramebuffers(1, &m_RendererID);
}

void FramebufferOGL::CreateAttachments()
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);

    // Color attachment — format depends on m_ColorFormat.
    const GLint  internalFmt = (m_ColorFormat == CHEngine::FramebufferColorFormat::RGBA16F)
                                   ? GL_RGBA16F : GL_RGBA8;
    const GLenum pixelType   = (m_ColorFormat == CHEngine::FramebufferColorFormat::RGBA16F)
                                   ? GL_FLOAT : GL_UNSIGNED_BYTE;

    glGenTextures(1, &m_ColorAttachment);
    glBindTexture(GL_TEXTURE_2D, m_ColorAttachment);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, (GLsizei)m_Width, (GLsizei)m_Height, 0,
                 GL_RGBA, pixelType, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, m_ColorAttachment, 0);

    // Depth+stencil renderbuffer
    glGenRenderbuffers(1, &m_DepthAttachment);
    glBindRenderbuffer(GL_RENDERBUFFER, m_DepthAttachment);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
                          (GLsizei)m_Width, (GLsizei)m_Height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, m_DepthAttachment);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        CHE_CORE_ERROR("FramebufferOGL: framebuffer incomplete! ({0}x{1})", m_Width, m_Height);
        CHE_CORE_ASSERT(false, "Failed to create valid framebuffer");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void FramebufferOGL::DeleteAttachments()
{
    if (m_ColorAttachment) { glDeleteTextures(1, &m_ColorAttachment);      m_ColorAttachment = 0; }
    if (m_DepthAttachment) { glDeleteRenderbuffers(1, &m_DepthAttachment); m_DepthAttachment = 0; }
}

void FramebufferOGL::Bind() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);
    glViewport(0, 0, (GLsizei)m_Width, (GLsizei)m_Height);
}

void FramebufferOGL::Unbind() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void FramebufferOGL::Resize(uint32_t width, uint32_t height)
{
    width  = std::max(width,  1u);
    height = std::max(height, 1u);
    if (m_Width == width && m_Height == height) return;

    m_Width  = width;
    m_Height = height;

    DeleteAttachments();
    CreateAttachments();
}

} // namespace CHModules
