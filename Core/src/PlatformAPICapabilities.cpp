#include "Core.h"
#include "EngineContext.h"

namespace CHEngine {
	constexpr RenderAPIStore GetDefaultRenderAPIStore()
	{
		RenderAPIStore store = 0;

		store |= BIT((int)ERenderAPI::OPENGL);
		store |= BIT((int)ERenderAPI::VULKAN);

#ifdef CHE_PLATFORM_WINDOWS
		store |= BIT((int)ERenderAPI::DIRECTX11);
		store |= BIT((int)ERenderAPI::DIRECTX12);
#endif

#ifdef CHE_PLATFORM_APPLE
		store |= BIT((int)ERenderAPI::METALL);
#endif

		return store;
	}

	void RenderAPICaps::Initialize()
	{
		static RenderAPIStore store = GetDefaultRenderAPIStore();
		g_EngineContext.RenderApiStore = &store;
	}

	void RenderAPICaps::SetFlag(WRenderAPI api, bool flag)
	{
		flag ? *(g_EngineContext.RenderApiStore) |= (BIT((int)api))
			: *(g_EngineContext.RenderApiStore) &= (~(BIT((int)api)));
	}

	bool RenderAPICaps::IsAvailable(WRenderAPI api)
	{
		return *(g_EngineContext.RenderApiStore) & (BIT((int)api));
	}

	BitwiseRange<WRenderAPI, RenderAPIStore> RenderAPICaps::AllAvailableAPI()
	{
		return BitwiseRange<WRenderAPI, RenderAPIStore>(*(g_EngineContext.RenderApiStore));
	}

	bool RenderAPICaps::HasAnyAPI()
	{
		return (RenderAPIStore)0 | *(g_EngineContext.RenderApiStore);
	}

	ModuleNames RenderAPICaps::GetModuleNames(WRenderAPI api)
	{
		switch (api.GetEnum())
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