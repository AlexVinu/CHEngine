#pragma once

#include "Core.h"
#include "Render/UISystem/IImGuiLayer.h"

namespace CHEngine
{
	class CHENGINE_API UIFacade
	{
	public:
		static void SetLayer(IImGuiLayer* layer);
		static IImGuiLayer* GetLayer();

		static void SetRenderContext(const RenderContextInfo& ctx);
		static void Begin();
		static void End();

	private:
		UIFacade() = delete;
		~UIFacade() = delete;
		UIFacade(const UIFacade&) = delete;
		UIFacade& operator=(const UIFacade&) = delete;
		UIFacade(UIFacade&&) = delete;
		UIFacade& operator=(UIFacade&&) = delete;
	};
}
