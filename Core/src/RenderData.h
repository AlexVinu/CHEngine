#pragma once

#include <cstring>

#include "EnumWrapper.h"

namespace CHEngine
{   
    struct ModuleNames
    {
        const char* Renderer;
        const char* ImGui;
    };

    enum class ERenderAPI
    {
        NONE = 0,
        OPENGL = 1,
        VULKAN = 2,
        METAL = 3,
        DIRECTX11 = 4,
        DIRECTX12 = 5
    };

    using WRenderAPI = EnumWrapper<ERenderAPI, 
        [](ERenderAPI api) {
        switch (api)
        {
            case ERenderAPI::OPENGL: return "OpenGL";
            case ERenderAPI::VULKAN: return "Vulkan";
            case ERenderAPI::METAL: return "Metal";
            case ERenderAPI::DIRECTX11: return "DirectX11";
            case ERenderAPI::DIRECTX12: return "DirectX12";
            default: return "None";
        }}>;
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
