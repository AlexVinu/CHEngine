#pragma once

#include <Core.h>

namespace CHEngine {
	enum class EventType
	{
		None = 0,
		WindowClose, WindowResize, WindowFocus, WindowLostFocus, WindowMoved,
		AppTick, AppUpdate, AppRender
	};

	enum EventCategory
	{
		None = 0,
		EventCategoryApplication = BIT(0)
	};
}
