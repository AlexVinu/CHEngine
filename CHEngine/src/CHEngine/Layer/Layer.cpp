#include "chepch.h"
#include "Layer.h"

#include "CHEngine/Application.h"

namespace CHEngine
{
	Layer::Layer(const std::string& name)
		: m_DebugName(name){ }

	Layer::~Layer()
	{
		
	}

	void Layer::QueueTransition(Scope<Layer> toLayer)
	{
		auto& layerStack = Application::Get().m_LayerStack;
		for (auto& layer : layerStack)
		{
			if (layer.get() == this)
			{
				layerStack.AddDeferredOperation([&layer, &toLayer]() {layer = std::move(toLayer); });
				return;
			}
		}
	}
}
