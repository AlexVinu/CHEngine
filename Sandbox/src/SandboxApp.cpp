#include <CHEngine.h>

//#include "Rendering/RendererOGL/src/VertexArrayOGL.h"
//#include "Rendering/RendererOGL/src/BufferOGL.h"
//#include "Rendering/RendererOGL/src/RenderApiOGL.h"
//#include "Rendering/RendererOGL/src/RendererOGL.h"
//#include "Rendering/RendererOGL/src/ShaderOGL.h"

#include <windows.h>
class ExampleLayer : public CHEngine::Layer
{
public:
	ExampleLayer()
		: Layer("Example") {

	}

	void OnUpdate() override
	{
		CHE_INFO("ExampleLayer::Update");
	}

	void OnEvent(CHEngine::Event& e) override
	{
		CHE_TRACE("{0}", e);
	}
};

class Sandbox : public CHEngine::Application
{
public:
	Sandbox()
	{
		PushLayer(new ExampleLayer());
	}

	~Sandbox()
	{

	}
};

CHEngine::Application* CHEngine::CreateApplication()
{
	return new Sandbox();
}
