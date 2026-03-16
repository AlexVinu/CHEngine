#pragma once

using ProcLoader = void* (*)(const char*);

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

    // По большей части это костыли
    // Если учитывать все особенности рендер апи, то много всего хранить придется + это не красиво
    struct NativeWindowData
    {
        void* WindowHandle = nullptr;
        void* DisplayHandle = nullptr;
    };

    struct RendererInitInfo
    {
        NativeWindowData NativeWindow;
        uint32_t Width = 0;
        uint32_t Height = 0;
        ProcLoader Loader = nullptr;
    };

}