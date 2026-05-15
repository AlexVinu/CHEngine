#include "chepch.h"
#include "LayerStack.h"

namespace CHEngine
{
	LayerStack::LayerStack()
	{
		m_LayerInsert = m_Layers.begin();
	}

	LayerStack::~LayerStack()
	{
		Clear();
	}

	void LayerStack::PushLayer(Layer* layer)
	{
		m_LayerInsert = m_Layers.emplace(m_LayerInsert, layer);
		++m_LayerInsert;
	}

	void LayerStack::PushOverlay(Layer* overlay)
	{
		m_Layers.emplace_back(overlay);
	}

	void LayerStack::PopLayer(Layer* layer)
	{
		auto it = std::find_if(m_Layers.begin(), m_Layers.end(),
			[layer](const std::unique_ptr<Layer>& ptr) { return ptr.get() == layer; });
		if (it != m_Layers.end())
		{
			const bool was_before_insert = it < m_LayerInsert;
			(*it)->OnDetach();
			m_Layers.erase(it);
			if (was_before_insert)
				--m_LayerInsert;
		}
	}

	void LayerStack::PopOverlay(Layer* overlay)
	{
		auto it = std::find(m_Layers.begin(), m_Layers.end(), overlay);
		if (it != m_Layers.end())
		{
			(*it)->OnDetach();
			it->reset();
			m_Layers.erase(it);
		}
	}

	void LayerStack::Clear()
	{
		for (Scope<Layer>& layer : m_Layers)
		{
			layer->OnDetach();
			layer.reset();
		}

		m_Layers.clear();
		m_LayerInsert = m_Layers.begin();
	}

	void LayerStack::Flush()
	{
		for (auto func : m_DeferredOps)
			func();
		m_DeferredOps.clear();
	}

	void LayerStack::AddDeferredOperation(std::function<void()> func)
	{
		m_DeferredOps.push_back(func);
	}

}
