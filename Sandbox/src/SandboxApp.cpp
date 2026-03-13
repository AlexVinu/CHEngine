#include <CHEngine.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <string>

class ExampleLayer : public CHEngine::Layer
{
public:
	ExampleLayer()
		: Layer("Example")
		, m_TintColor{ 1.0f, 1.0f, 1.0f, 1.0f }
		, m_Camera(45.0f, 0.1f, 100.0f)
		, m_LastTime(std::chrono::steady_clock::now())
	{
	}

	void OnUpdate() override
	{
		// --- Delta time ---
		auto now = std::chrono::steady_clock::now();
		float dt = std::chrono::duration<float>(now - m_LastTime).count();
		m_LastTime = now;

		// --- Auto-rotate ---
		if (m_AutoRotate)
		{
			m_RotX += m_SpeedX * dt;
			m_RotY += m_SpeedY * dt;
			m_RotZ += m_SpeedZ * dt;
		}

		auto& app = CHEngine::Application::Get();
		auto* shader = app.GetRenderResources().Get(app.GetActiveShader());
		if (!shader) return;

		// --- u_Color tint ---
		shader->SetFloat4(CHEngine::String("u_Color"),
			m_TintColor[0], m_TintColor[1], m_TintColor[2], m_TintColor[3]);

		// --- u_ViewProjection ---
		glm::mat4 vp = m_Camera.GetViewProjectionMatrix(m_AspectRatio);
		shader->SetMat4(CHEngine::String("u_ViewProjection"), glm::value_ptr(vp));

		// --- u_Transform (model matrix) ---
		glm::mat4 transform = glm::mat4(1.0f);
		transform = glm::rotate(transform, glm::radians(m_RotX), glm::vec3(1.0f, 0.0f, 0.0f));
		transform = glm::rotate(transform, glm::radians(m_RotY), glm::vec3(0.0f, 1.0f, 0.0f));
		transform = glm::rotate(transform, glm::radians(m_RotZ), glm::vec3(0.0f, 0.0f, 1.0f));
		shader->SetMat4(CHEngine::String("u_Transform"), glm::value_ptr(transform));
	}

	void OnImGuiRender() override
	{
		// Update aspect ratio from current window size
		ImVec2 displaySize = ImGui::GetIO().DisplaySize;
		if (displaySize.y > 0.0f)
			m_AspectRatio = displaySize.x / displaySize.y;

		// ---- Debug info ----
		ImGui::Begin("CHEngine Debug");
		ImGui::Text("%.3f ms/frame  (%.1f FPS)",
			1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::End();

		// ---- Camera ----
		ImGui::Begin("Camera");

		glm::vec3 pos = m_Camera.GetPosition();
		float yaw = m_Camera.GetYaw();
		float pitch = m_Camera.GetPitch();
		float fov = m_Camera.GetFOV();

		bool changed = false;
		changed |= ImGui::DragFloat3("Position", glm::value_ptr(pos), 0.05f);
		changed |= ImGui::SliderFloat("Yaw", &yaw, -180.0f, 180.0f, "%.1f°");
		changed |= ImGui::SliderFloat("Pitch", &pitch, -89.0f, 89.0f, "%.1f°");
		changed |= ImGui::SliderFloat("FOV", &fov, 10.0f, 120.0f, "%.1f°");

		if (changed)
		{
			m_Camera.SetPosition(pos);
			m_Camera.SetYaw(yaw);
			m_Camera.SetPitch(pitch);
			m_Camera.SetFOV(fov);
		}

		if (ImGui::Button("Reset camera"))
		{
			m_Camera.SetPosition({ 0.0f, 0.0f, 3.0f });
			m_Camera.SetYaw(-90.0f);
			m_Camera.SetPitch(0.0f);
			m_Camera.SetFOV(45.0f);
		}

		ImGui::Separator();
		ImGui::Text("Forward: (%.2f, %.2f, %.2f)", m_Camera.GetForward().x,
			m_Camera.GetForward().y,
			m_Camera.GetForward().z);
		ImGui::End();

		// ---- Cube ----
		ImGui::Begin("Cube");

		ImGui::SeparatorText("Rotation");
		ImGui::Checkbox("Auto rotate", &m_AutoRotate);

		ImGui::BeginDisabled(!m_AutoRotate);
		ImGui::SliderFloat("Speed X°/s", &m_SpeedX, -180.0f, 180.0f, "%.0f");
		ImGui::SliderFloat("Speed Y°/s", &m_SpeedY, -180.0f, 180.0f, "%.0f");
		ImGui::SliderFloat("Speed Z°/s", &m_SpeedZ, -180.0f, 180.0f, "%.0f");
		ImGui::EndDisabled();

		ImGui::Spacing();
		ImGui::DragFloat("Angle X", &m_RotX, 1.0f, -360.0f, 360.0f, "%.1f°");
		ImGui::DragFloat("Angle Y", &m_RotY, 1.0f, -360.0f, 360.0f, "%.1f°");
		ImGui::DragFloat("Angle Z", &m_RotZ, 1.0f, -360.0f, 360.0f, "%.1f°");

		if (ImGui::Button("Reset rotation"))
			m_RotX = m_RotY = m_RotZ = 0.0f;

		ImGui::End();

		// ---- Color ----
		ImGui::Begin("Color");
		ImGui::Text("Tint color:");
		ImGui::ColorEdit4("##tint", m_TintColor);
		if (ImGui::Button("Reset"))
			m_TintColor[0] = m_TintColor[1] = m_TintColor[2] = m_TintColor[3] = 1.0f;
		ImGui::End();

		// ---- Shader Manager ----
		ImGui::Begin("Shader Manager");

		auto& app = CHEngine::Application::Get();
		auto& res = app.GetRenderResources();
		const auto& entries = res.GetShaderEntries();
		CHEngine::ShaderHandle activeShader = app.GetActiveShader();

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

	void OnEvent(CHEngine::Event& e) override {
		CHEngine::EventDispatcher dispatcher(e);
		dispatcher.Dispatch<CHEngine::KeyPressedEvent>(std::bind(&ExampleLayer::OnKeyPressedEvent, this, std::placeholders::_1));
	}
	bool OnKeyPressedEvent(CHEngine::KeyPressedEvent& e)
	{
		auto pos = m_Camera.GetPosition();
		switch(e.GetKeyCode())
		{
		case 87: m_Camera.SetPosition(glm::vec3(pos.x, pos.y, pos.z + 1)); break;
		case 83: m_Camera.SetPosition(glm::vec3(pos.x, pos.y, pos.z - 1)); break;
		}
		return false;
	}
private:
	float           m_TintColor[4];
	float           m_AspectRatio = 16.0f / 9.0f;
	CHEngine::Camera m_Camera;

	// Rotation state
	float m_RotX = 0.0f;
	float m_RotY = 0.0f;
	float m_RotZ = 0.0f;
	bool  m_AutoRotate = true;
	float m_SpeedX = 0.0f;   // degrees / second
	float m_SpeedY = 30.0f;
	float m_SpeedZ = 0.0f;

	std::chrono::steady_clock::time_point m_LastTime;
};

class Sandbox : public CHEngine::Application
{
public:
	Sandbox()
	{
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