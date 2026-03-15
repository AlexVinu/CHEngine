#pragma once

#include<Core.h>
#include "CHEngine/Events/Event.h"

namespace CHEngine
{
	class CHENGINE_API Layer
	{
	public:
		Layer(const std::string& name = "Layer");
		virtual ~Layer();

		virtual void OnAttach() {}
		virtual void OnDetach() {}
		virtual void OnUpdate(float /*dt*/) {}
		virtual void OnEvent(Event& /*event*/) {}
		virtual void OnImGuiRender() {}

		inline const std::string& GetName() const { return m_DebugName; }

	protected:
		std::string m_DebugName;
	};
}
