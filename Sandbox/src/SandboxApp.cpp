#include <CHEngine.h>

#include <string>

class ExampleLayer : public CHEngine::Layer
{
public:
	ExampleLayer()
		: Layer("Example")
		, m_TintColor{ 1.0f, 1.0f, 1.0f, 1.0f }   // white = no tint
	{
	}

	void OnUpdate() override
	{
		// Set u_Color uniform on the active shader every frame.
		// The shader is already bound by Application::Run() before OnUpdate().
		auto& app    = CHEngine::Application::Get();
		auto* shader = app.GetRenderResources().Get(app.GetActiveShader());
		if (shader)
			shader->SetFloat4(CHEngine::String("u_Color"),
				m_TintColor[0], m_TintColor[1], m_TintColor[2], m_TintColor[3]);
	}

	void OnImGuiRender() override
	{
		auto& app = CHEngine::Application::Get();
		auto& res = app.GetRenderResources();
		const auto& entries = res.GetShaderEntries();
		CHEngine::ShaderHandle activeShader = app.GetActiveShader();

		// ---- Debug info ----
		ImGui::Begin("CHEngine Debug");
		ImGui::Text("%.3f ms/frame  (%.1f FPS)",
			1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::End();

		// ---- Color Control ----
		ImGui::Begin("Color");
		ImGui::Text("Tint color:");
		ImGui::ColorEdit4("##tint", m_TintColor);
		if (ImGui::Button("Reset"))
			m_TintColor[0] = m_TintColor[1] = m_TintColor[2] = m_TintColor[3] = 1.0f;
		ImGui::End();

		// ---- Shader Manager ----
		ImGui::Begin("Shader Manager");

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

			if (!e.valid)
				ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[ERR] %s", e.name.c_str());
			else if (isActive)
				ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), " [*]  %s", e.name.c_str());
			else
				ImGui::Text("      %s", e.name.c_str());

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

			ImGui::SameLine();
			if (ImGui::Button(("Reload##reload" + std::to_string(i)).c_str(), ImVec2(60, 0)))
				res.ReloadShader(e.handle);
		}

		ImGui::End();
	}

	void OnEvent(CHEngine::Event& e) override
	{
	}

private:
	float m_TintColor[4];   // RGBA, passed as u_Color uniform each frame
};

class Sandbox : public CHEngine::Application
{
public:
	Sandbox()
	{
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

	~Sandbox() {}
};

CHEngine::Application* CHEngine::CreateApplication()
{
	return new Sandbox();
}
