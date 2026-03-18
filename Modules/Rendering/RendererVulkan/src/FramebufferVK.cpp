#include "FramebufferVK.h"

#include <algorithm>

namespace CHModules {

    FramebufferVK::FramebufferVK(uint32_t width, uint32_t height)
        : m_Width(std::max(width, 1u)), m_Height(std::max(height, 1u))
    {
        // TODO: создать off-screen VkFramebuffer
    }

    FramebufferVK::~FramebufferVK()
    {
        // TODO: cleanup
    }

    void FramebufferVK::Bind() const {}
    void FramebufferVK::Unbind() const {}

    void FramebufferVK::Resize(uint32_t width, uint32_t height)
    {
        m_Width  = std::max(width,  1u);
        m_Height = std::max(height, 1u);
        // TODO: recreate framebuffer
    }

}
