#include <CHEngine.h>

#include <string>

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
		// ---- Debug info ----
		ImGui::Begin("CHEngine Debug");
		ImGui::Text("%.3f ms/frame  (%.1f FPS)",
			1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::End();

		// ---- Shader Manager ----
		ImGui::Begin("Shader Manager");

		auto& app           = CHEngine::Application::Get();
		auto& res           = app.GetRenderResources();
		const auto& entries = res.GetShaderEntries();
		CHEngine::ShaderHandle activeShader = app.GetActiveShader();

		// Show active shader name
		const char* activeName = "(unknown)";
		for (const auto& e : entries)
			if (e.handle == activeShader) { activeName = e.name.c_str(); break; }
		ImGui::Text("Active: %s", activeName);

		ImGui::Separator();
		ImGui::Spacing();

		for (int i = 0; i < static_cast<int>(entries.size()); i++)
		{
			const auto& e = entries[i];
			bool isActive = (e.handle == activeShader);

			// Shader name with status colour
			if (!e.valid)
				ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[ERR] %s", e.name.c_str());
			else if (isActive)
				ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), " [*]  %s", e.name.c_str());
			else
				ImGui::Text("      %s", e.name.c_str());

			// "Use" button (disabled when already active)
			ImGui::SameLine(160.0f);
			if (isActive)
			{
				ImGui::BeginDisabled();
				ImGui::Button(("In use##use" + std::to_string(i)).c_str(), ImVec2(60, 0));
				ImGui::EndDisabled();
			}
			else
			{
				if (ImGui::Button(("Use##use" + std::to_string(i)).c_str(), ImVec2(60, 0)))
					app.SetActiveShader(e.handle);
			}

			// "Reload" button — re-reads files and recompiles
			ImGui::SameLine();
			if (ImGui::Button(("Reload##reload" + std::to_string(i)).c_str(), ImVec2(60, 0)))
				res.ReloadShader(e.handle);
		}

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
		// Register additional demo shaders
		// ("Basic" is already registered by Application base class)
		GetRenderResources().CreateShaderFromFile(
			CHEngine::String("Flat"),
			CHEngine::String("shaders/flat.vert"),
			CHEngine::String("shaders/flat.frag")
		);
		GetRenderResources().CreateShaderFromFile(
			CHEngine::String("Neon"),
			CHEngine::String("shaders/neon.vert"),
			CHEngine::String("shaders/neon.frag")
		);

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
