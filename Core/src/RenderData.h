#pragma once

namespace CHEngine
{
	enum class ERenderAPI
	{
		NONE		= 0,
		OPENGL		= 1,
		VULKAN		= 2,
		METALL		= 3,
		DIRECTX11	= 4,
		DIRECTX12	= 5
	};

    struct RendererInitInfo
    {
        union
        {
            struct {
                void* Loader = nullptr;
            } OpenGL;
            struct
            {
                void* WindowHandle = nullptr;
                void* DisplayHandle = nullptr;
            } Vulkan;

            struct
            {
                void* WindowHandle = nullptr;
                void* DisplayHandle = nullptr;
            } DirectX;
            struct
            {
                void* NSView = nullptr;        
                void* MetalLayer = nullptr;    
                void* Device = nullptr;        
            } Metal;
        };
    };

}