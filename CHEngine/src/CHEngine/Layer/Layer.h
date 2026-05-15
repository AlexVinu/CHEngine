#pragma once

#include <Core.h>
#include <CheStl/MemoryTypes.h>
#include "CHEngine/Events/Event.h"
#include "Timestep.h"

namespace CHEngine
{
	class CHENGINE_API Layer
	{
	public:
		Layer(const std::string& name = "Layer");
		virtual ~Layer();

		virtual void OnAttach() {}
		virtual void OnDetach() {}
		virtual void OnUpdate(Timestep /*dt*/) {}
		virtual void OnEvent(Event& /*event*/) {}
		virtual void OnImGuiRender() {}

		inline const std::string& GetName() const { return m_DebugName; }

		// Queue Transition
		template<std::derived_from<Layer> T, typename... Args>
		void TransitionTo(Args&&... args)
		{
			QueueTransition(std::move(std::make_unique<T>(std::forward<Args>(args)...)));
		}

	protected:
		std::string m_DebugName;

	private:
		void QueueTransition(Scope<Layer> layer);
	};
}
