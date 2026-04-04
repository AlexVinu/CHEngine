#include "chepch.h"

#include "UIFacade.h"

namespace CHEngine
{
	namespace
	{
		IImGuiLayer* s_Layer = nullptr;
	}

	void UIFacade::SetLayer(IImGuiLayer* layer)
	{
		s_Layer = layer;
	}

	IImGuiLayer* UIFacade::GetLayer()
	{
		return s_Layer;
	}

	void UIFacade::SetRenderContext(const RenderContextInfo& ctx)
	{
		if (s_Layer)
			s_Layer->SetRenderContext(ctx);
	}

	void UIFacade::Begin()
	{
		if (s_Layer)
			s_Layer->Begin();
	}

	void UIFacade::End()
	{
		if (s_Layer)
			s_Layer->End();
	}
}
