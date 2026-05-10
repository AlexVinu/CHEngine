#include "PlatformAPICapabilities.h"
#include <cstring>

namespace CHEngine {
	bool RenderModuleResolver::TryParseRenderAPI(const char* value, ERenderAPI* out_api)
	{
		if (!value || !out_api)
			return false;

		if (std::strcmp(value, "vulkan") == 0)
		{
			*out_api = ERenderAPI::VULKAN;
			return true;
		}
		if (std::strcmp(value, "metal") == 0)
		{
			*out_api = ERenderAPI::METAL;
			return true;
		}
		if (std::strcmp(value, "opengl") == 0)
		{
			*out_api = ERenderAPI::OPENGL;
			return true;
		}
		return false;
	}

	const char* RenderModuleResolver::ToConfigName(ERenderAPI api)
	{
		switch (api)
		{
			case ERenderAPI::VULKAN: return "vulkan";
			case ERenderAPI::METAL: return "metal";
			default: return "opengl";
		}
	}

	ModuleNames RenderModuleResolver::GetModuleNames(ERenderAPI api)
	{
		switch (api)
		{
		case ERenderAPI::OPENGL:
#if defined(CHE_PLATFORM_WINDOWS)
			return { "RendererOGL.dll", "ImGuiOGL.dll" };
#elif defined(CHE_PLATFORM_APPLE)
			return { "libRendererOGL.dylib", "libImGuiOGL.dylib" };
#else
			return { "libRendererOGL.so", "libImGuiOGL.so" };
#endif

		case ERenderAPI::VULKAN:
#if defined(CHE_PLATFORM_WINDOWS)
			return { "RendererVK.dll", "ImGuiVK.dll" };
#elif defined(CHE_PLATFORM_APPLE)
			return { "libRendererVK.dylib", "libImGuiVK.dylib" };
#else
			return { "libRendererVK.so", "libImGuiVK.so" };
#endif

		case ERenderAPI::METAL:
#if defined(CHE_PLATFORM_APPLE)
			return { "libRendererMTL.dylib", "libImGuiMTL.dylib" };
#else
			return { nullptr, nullptr };
#endif

		default:
			return { nullptr, nullptr };
		}
	}

	bool RenderModuleResolver::IsSupportedOnPlatform(ERenderAPI api)
	{
		const ModuleNames module_names = GetModuleNames(api);
		return module_names.Renderer && module_names.ImGui;
	}
}