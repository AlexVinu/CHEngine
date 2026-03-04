#include <CHEngine.h>

class ExampleLayer : public CHEngine::Layer
{
public:
	ExampleLayer()
		: Layer("Example")
	{
	}

	void OnUpdate() override
	{
	}

	void OnImGuiRender() override
	{
		ImGui::Begin("CHEngine Debug");
		ImGui::Text("Hello from ExampleLayer!");
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
			1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::End();
	}

	void OnEvent(CHEngine::Event& e) override
	{
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
