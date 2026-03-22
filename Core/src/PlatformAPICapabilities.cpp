#include "Core.h"
#include "EngineContext.h"

namespace CHEngine {
	void RenderAPICaps::Initialize()
	{
		static RenderAPIStore store = 0;
		// API available if it exists on this system
		store |= BIT((int)ERenderAPI::OPENGL);
		store |= BIT((int)ERenderAPI::VULKAN);
#ifdef CHE_PLATFORM_WINDOWS
		store |= BIT((int)ERenderAPI::DIRECTX11);
		store |= BIT((int)ERenderAPI::DIRECTX12);
#endif 
#ifdef CHE_PLATFORM_APPLE
		store |= BIT((int)ERenderAPI::METALL);
#endif

		g_EngineContext.RenderApiStore = &store;
	}

	void RenderAPICaps::SetFlag(ERenderAPI api, bool flag)
	{
		flag ? *(g_EngineContext.RenderApiStore) |= (BIT((int)api))
			: *(g_EngineContext.RenderApiStore) &= (~(BIT((int)api)));
	}

	bool RenderAPICaps::IsAvailable(ERenderAPI api)
	{
		return *(g_EngineContext.RenderApiStore) & (BIT((int)api));
	}

	BitwiseRange<ERenderAPI, RenderAPIStore> RenderAPICaps::AllAvailableAPI()
	{
		return BitwiseRange<ERenderAPI, RenderAPIStore>(*(g_EngineContext.RenderApiStore));
	}

	bool RenderAPICaps::HasAnyAPI()
	{
		return (RenderAPIStore)0 | *(g_EngineContext.RenderApiStore);
	}

	ModuleNames RenderAPICaps::GetModuleNames(ERenderAPI api)
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
}