#pragma once

#include "Core.h"
#include "Layer.h"

#include "CheStl/MemoryTypes.h"

#include <vector>
#include <functional>

namespace CHEngine
{
	class Application;

	class CHENGINE_API LayerStack
	{
	public:
		LayerStack();
		~LayerStack();

		LayerStack(const LayerStack&) = delete;
		LayerStack& operator=(const LayerStack&) = delete;

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* overlay);
		void PopLayer(Layer* layer);
		void PopOverlay(Layer* overlay);
		void Clear();

		void AddDeferredOperation(std::function<void()>);

		std::vector<Scope<Layer>>::iterator begin() { return m_Layers.begin(); }
		std::vector<Scope<Layer>>::iterator end() { return m_Layers.end(); }
	private:
		friend Application; friend Layer;

		void Flush();
		std::vector<std::function<void()>> m_DeferredOps;

		std::vector<Scope<Layer>> m_Layers;
		std::vector<Scope<Layer>>::iterator m_LayerInsert;
	};

}
