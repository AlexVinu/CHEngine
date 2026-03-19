#pragma once

#include <cstring>

namespace CHEngine
{
	enum class ERenderAPI
	{
		NONE		= 0,
		OPENGL		= 1,
		VULKAN		= 2,
		METAL		= 3,
		DIRECTX11	= 4,
		DIRECTX12	= 5
	};

    // ─── Инициализационные данные для конкретного рендер-бэкенда ──────────────
    // Передаётся из Window → Renderer при инициализации.
    // Активен ровно один член union — выбирается по ERenderAPI.

    struct OpenGLInitData
    {
        void* Loader;           // GLADloadproc (glfwGetProcAddress)
    };

    struct VulkanInitData
    {
        void* WindowHandle;
        void* DisplayHandle;
    };

    struct DirectXInitData
    {
        void* WindowHandle;
        void* DisplayHandle;
    };

    struct MetalInitData
    {
        void* NSWindow;     // NSWindow* (от glfwGetCocoaWindow), НЕ NSView
        void* MetalLayer;
        void* Device;
    };

    struct RendererInitInfo
    {
        union
        {
            OpenGLInitData  OpenGL;
            VulkanInitData  Vulkan;
            DirectXInitData DirectX;
            MetalInitData   Metal;
        };

        // Конструктор по умолчанию — зануляет весь union
        RendererInitInfo() { std::memset(this, 0, sizeof(*this)); }
    };

}
