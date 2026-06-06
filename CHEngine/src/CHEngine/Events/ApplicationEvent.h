#pragma once

#include "Event.h"

#include <sstream>

namespace CHEngine {

	class CHENGINE_API WindowResizeEvent : public Event
	{
	public:
		WindowResizeEvent(unsigned int width, unsigned int height)
			: m_Width(width), m_Height(height){ }

		inline unsigned int GetWidth() const { return m_Width; }
		inline unsigned int GetHeight() const { return m_Height; }

		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "WindowResizeEvent: " << m_Width << ", " << m_Height;
			return ss.str();
		}

		EVENT_CLASS_TYPE(WindowResize)
		EVENT_CLASS_CATEGORY(EventCategoryApplication)
	private:
		unsigned int m_Width, m_Height;
	};

	class CHENGINE_API WindowCloseEvent : public Event
	{
	public:
		WindowCloseEvent(){}

		EVENT_CLASS_TYPE(WindowClose)
		EVENT_CLASS_CATEGORY(EventCategoryApplication)
	};

	class CHENGINE_API WindowMinimizedEvent : public Event
	{
	public:
		WindowMinimizedEvent(bool minimized) : m_Minimized(minimized) {}

		inline bool IsMinimized() const { return m_Minimized; }

		EVENT_CLASS_TYPE(WindowMinimized)
		EVENT_CLASS_CATEGORY(EventCategoryApplication)
	private:
		bool m_Minimized;
	};
}
