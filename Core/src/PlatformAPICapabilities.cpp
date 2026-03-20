#include "Core.h"
#include "EngineContext.h"

void CHEngine::RenderAPICaps::Initialize()
{
	static RenderAPIStore store = 0;
	// API available if it exists on this system + there are required drivers(not realized)
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

bool CHEngine::RenderAPICaps::IsAvailable(ERenderAPI api)
{
	return *(g_EngineContext.RenderApiStore) & (BIT((int)api));
}

bool CHEngine::RenderAPICaps::HasAnyAPI()
{
	return (int)0 | *(g_EngineContext.RenderApiStore);
}
