#pragma once

#include <CHEngine.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <string>
#include <cstring>

class SceneViewLayer : public CHEngine::Layer
{
public:
	SceneViewLayer()
		: Layer("SceneView")
		, m_Camera(45.0f, 0.1f, 500.0f)
		, m_LastTime(std::chrono::steady_clock::now())
	{
		auto& app = CHEngine::Application::Get();
		auto& res = app.GetRenderResources();
		m_RenderApi = app.GetRenderApiHandle();

		m_MeshShader = res.CreateShaderFromFile(
			CHEngine::String("Mesh"),
			CHEngine::String("shaders/mesh.vert"),
			CHEngine::String("shaders/mesh.frag")
		);
		m_GridShader = res.CreateShaderFromFile(
			CHEngine::String("Grid"),
			CHEngine::String("shaders/grid.vert"),
			CHEngine::String("shaders/grid.frag")
		);

		BuildGrid();
		ApplyOrbit(); // set initial camera position from orbit params
	}

	void OnUpdate() override
	{
		auto now = std::chrono::steady_clock::now();
		float dt = std::chrono::duration<float>(now - m_LastTime).count();
		m_LastTime = now;
		(void)dt;

		RenderScene();
	}

	void OnImGuiRender() override
	{
		ImVec2 displaySize = ImGui::GetIO().DisplaySize;
		if (displaySize.y > 0.0f)
			m_AspectRatio = displaySize.x / displaySize.y;

		ImGuizmo::BeginFrame();

		UpdateCameraInput();

		DrawToolbar();
		DrawHierarchyPanel();
		DrawPropertiesPanel();
		DrawCameraPanel();
		DrawGizmo();
	}

	void OnEvent(CHEngine::Event& e) override {}

private:
	// =========================================================================
	// Orbit camera helpers
	// =========================================================================

	// Recompute camera position from orbit target + yaw/pitch + distance
	void ApplyOrbit()
	{
		glm::vec3 fwd = m_Camera.GetForward();
		m_Camera.SetPosition(m_OrbitTarget - fwd * m_OrbitDist);
	}

	void SetViewPreset(float yaw, float pitch)
	{
		m_Camera.SetYaw(yaw);
		m_Camera.SetPitch(pitch);
		ApplyOrbit();
	}

	void FocusOnSelected()
	{
		CHEngine::SceneObject* obj = m_Scene.FindByID(m_SelectedObjectID);
		if (!obj) return;

		m_OrbitTarget = obj->ObjectTransform.Position;

		// Estimate bounding radius from mesh vertices (local space)
		float maxR = 0.5f;
		for (auto& mesh : obj->Meshes)
			for (auto& v : mesh.GetVertices())
			{
				float d = glm::length(v.Position);
				if (d > maxR) maxR = d;
			}

		float scaleMax = std::max({ obj->ObjectTransform.Scale.x,
		                             obj->ObjectTransform.Scale.y,
		                             obj->ObjectTransform.Scale.z });
		m_OrbitDist = glm::clamp(maxR * scaleMax * 2.5f, 1.0f, 200.0f);
		ApplyOrbit();
	}

	// =========================================================================
	// Per-frame camera input (called from OnImGuiRender after NewFrame)
	// =========================================================================
	void UpdateCameraInput()
	{
		ImGuiIO& io = ImGui::GetIO();

		// Only interact when mouse is NOT captured by an ImGui widget
		// (allow scroll anywhere — useful to zoom without hovering viewport)
		const bool freeViewport = !io.WantCaptureMouse;

		// --- Right-click drag: orbit ---
		if (ImGui::IsMouseDragging(ImGuiMouseButton_Right, 1.0f))
		{
			float sens = 0.25f;
			m_Camera.SetYaw  (m_Camera.GetYaw()   + io.MouseDelta.x * sens);
			m_Camera.SetPitch(m_Camera.GetPitch() - io.MouseDelta.y * sens);
			ApplyOrbit();
		}

		// --- Scroll: zoom (distance to target) ---
		if (io.MouseWheel != 0.0f)
		{
			float factor = 1.0f - io.MouseWheel * 0.12f;
			m_OrbitDist = glm::clamp(m_OrbitDist * factor, 0.3f, 500.0f);
			ApplyOrbit();
		}

		// --- Middle-click drag: pan orbit target ---
		if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f))
		{
			float panScale = m_OrbitDist * 0.0015f;
			glm::vec3 right = m_Camera.GetRight();
			glm::vec3 up    = m_Camera.GetUp();
			m_OrbitTarget -= right * io.MouseDelta.x * panScale;
			m_OrbitTarget += up    * io.MouseDelta.y * panScale;
			ApplyOrbit();
		}

		// --- F key: frame selected object ---
		if (ImGui::IsKeyPressed(ImGuiKey_F) && m_SelectedObjectID != 0)
			FocusOnSelected();

		// --- Follow object: keep orbit target locked to selection ---
		if (m_FollowObject && m_SelectedObjectID != 0)
		{
			auto* obj = m_Scene.FindByID(m_SelectedObjectID);
			if (obj)
			{
				m_OrbitTarget = obj->ObjectTransform.Position;
				ApplyOrbit();
			}
		}
	}

	// =========================================================================
	// Grid / axis geometry
	// =========================================================================
	void BuildGrid()
	{
		auto& res = CHEngine::Application::Get().GetRenderResources();

		struct LineVertex { glm::vec3 pos, color; };
		std::vector<LineVertex> verts;
		std::vector<uint32_t>  indices;

		const int   N      = 10;
		const glm::vec3 gridDim(0.22f, 0.22f, 0.22f);
		const glm::vec3 grid5  (0.38f, 0.38f, 0.38f);
		const glm::vec3 xColor (0.80f, 0.18f, 0.18f);
		const glm::vec3 yColor (0.18f, 0.75f, 0.18f);
		const glm::vec3 zColor (0.18f, 0.35f, 0.85f);

		auto addLine = [&](glm::vec3 a, glm::vec3 b, glm::vec3 col)
		{
			uint32_t i = static_cast<uint32_t>(verts.size());
			verts.push_back({ a, col });
			verts.push_back({ b, col });
			indices.push_back(i);
			indices.push_back(i + 1);
		};

		for (int i = -N; i <= N; i++)
		{
			if (i == 0) continue;
			glm::vec3 col = (i % 5 == 0) ? grid5 : gridDim;
			addLine({ -(float)N, 0.0f, (float)i }, { (float)N, 0.0f, (float)i }, col);
			addLine({ (float)i, 0.0f, -(float)N }, { (float)i, 0.0f, (float)N }, col);
		}

		const float ext = (float)N + 0.5f;
		addLine({ -ext, 0.0f, 0.0f }, {  ext, 0.0f, 0.0f }, xColor);
		addLine({ 0.0f, -ext, 0.0f }, { 0.0f,  ext, 0.0f }, yColor);
		addLine({ 0.0f, 0.0f, -ext }, { 0.0f, 0.0f,  ext }, zColor);

		std::vector<float> flat;
		flat.reserve(verts.size() * 6);
		for (auto& v : verts)
		{
			flat.push_back(v.pos.x);   flat.push_back(v.pos.y);   flat.push_back(v.pos.z);
			flat.push_back(v.color.r); flat.push_back(v.color.g); flat.push_back(v.color.b);
		}

		m_GridVAO = res.CreateVertexArray();
		auto vb = res.CreateVertexBuffer(flat.data(),
			static_cast<uint32_t>(flat.size() * sizeof(float)));
		CHEngine::BufferLayout layout = {
			{ CHEngine::ShaderDataType::Float3, "a_Position" },
			{ CHEngine::ShaderDataType::Float3, "a_Color"    },
		};
		vb->SetLayout(layout);
		res.Get(m_GridVAO)->AddVertexBuffer(vb);
		auto ib = res.CreateIndexBuffer(indices.data(),
			static_cast<uint32_t>(indices.size()));
		res.Get(m_GridVAO)->SetIndexBuffer(ib);
	}

	// =========================================================================
	// Rendering
	// =========================================================================
	void RenderScene()
	{
		auto& app = CHEngine::Application::Get();
		auto& res = app.GetRenderResources();
		auto* api = res.Get(m_RenderApi);
		if (!api) return;

		glm::mat4 vp = m_Camera.GetViewProjectionMatrix(m_AspectRatio);

		// Grid & axes
		if (m_ShowGrid)
		{
			auto* gs  = res.Get(m_GridShader);
			auto* gva = res.Get(m_GridVAO);
			if (gs && gva)
			{
				gs->Bind();
				gs->SetMat4(CHEngine::String("u_ViewProjection"), glm::value_ptr(vp));
				api->DrawLines(gva);
			}
		}

		// Meshes
		auto* shader = res.Get(m_MeshShader);
		if (!shader) return;

		shader->Bind();
		shader->SetMat4  (CHEngine::String("u_ViewProjection"), glm::value_ptr(vp));
		shader->SetFloat3(CHEngine::String("u_LightDir"),       -0.3f, -1.0f, -0.5f);
		shader->SetFloat3(CHEngine::String("u_LightColor"),      1.0f,  1.0f,  0.95f);
		shader->SetFloat3(CHEngine::String("u_AmbientColor"),    0.15f, 0.15f, 0.2f);

		for (auto& obj : m_Scene.GetObjects())
		{
			if (!obj->Visible) continue;

			float t[3] = { obj->ObjectTransform.Position.x, obj->ObjectTransform.Position.y, obj->ObjectTransform.Position.z };
			float r[3] = { obj->ObjectTransform.Rotation.x, obj->ObjectTransform.Rotation.y, obj->ObjectTransform.Rotation.z };
			float s[3] = { obj->ObjectTransform.Scale.x,    obj->ObjectTransform.Scale.y,    obj->ObjectTransform.Scale.z    };
			float raw[16];
			ImGuizmo::RecomposeMatrixFromComponents(t, r, s, raw);
			glm::mat4 model     = glm::make_mat4(raw);
			glm::mat4 normalMat = glm::transpose(glm::inverse(model));

			shader->SetMat4  (CHEngine::String("u_Transform"),    glm::value_ptr(model));
			shader->SetMat4  (CHEngine::String("u_NormalMatrix"), glm::value_ptr(normalMat));
			shader->SetFloat4(CHEngine::String("u_Color"),
				obj->Color.r, obj->Color.g, obj->Color.b, obj->Color.a);
			shader->SetFloat (CHEngine::String("u_Selected"),
				(obj->ID == m_SelectedObjectID) ? 1.0f : 0.0f);

			for (auto& mesh : obj->Meshes)
			{
				auto* vao = res.Get(mesh.GetVertexArray());
				if (vao) api->DrawIndexed(vao);
			}
		}
	}

	// =========================================================================
	// UI panels
	// =========================================================================
	void DrawToolbar()
	{
		ImGui::Begin("Toolbar");

		if (ImGui::RadioButton("Translate (W)", m_GizmoOperation == ImGuizmo::TRANSLATE))
			m_GizmoOperation = ImGuizmo::TRANSLATE;
		ImGui::SameLine();
		if (ImGui::RadioButton("Rotate (E)", m_GizmoOperation == ImGuizmo::ROTATE))
			m_GizmoOperation = ImGuizmo::ROTATE;
		ImGui::SameLine();
		if (ImGui::RadioButton("Scale (R)", m_GizmoOperation == ImGuizmo::SCALE))
			m_GizmoOperation = ImGuizmo::SCALE;

		ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();
		if (ImGui::Checkbox("Local", &m_LocalMode))
			m_GizmoMode = m_LocalMode ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

		ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();
		ImGui::Checkbox("Grid", &m_ShowGrid);

		ImGui::End();
	}

	void DrawHierarchyPanel()
	{
		ImGui::Begin("Scene Hierarchy");

		if (ImGui::Button("Import Model..."))
		{
			std::string path = CHEngine::FileDialog::OpenFile(
				"3D Models (*.obj, *.glb, *.gltf)", "");
			if (!path.empty())
				ImportModel(path);
		}

		ImGui::Separator();

		uint32_t deleteID = 0;
		for (auto& obj : m_Scene.GetObjects())
		{
			bool isSelected = (obj->ID == m_SelectedObjectID);
			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth;
			if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

			bool opened = ImGui::TreeNodeEx(
				(void*)(intptr_t)obj->ID, flags, "%s", obj->Name.c_str());

			if (ImGui::IsItemClicked())
				m_SelectedObjectID = obj->ID;

			if (ImGui::BeginPopupContextItem())
			{
				if (ImGui::MenuItem("Delete"))    deleteID = obj->ID;
				if (ImGui::MenuItem("Focus (F)")) FocusOnSelected();
				ImGui::EndPopup();
			}

			if (opened) ImGui::TreePop();
		}

		if (deleteID > 0)
		{
			if (m_SelectedObjectID == deleteID) m_SelectedObjectID = 0;
			m_Scene.RemoveObject(deleteID);
		}

		ImGui::End();
	}

	void DrawPropertiesPanel()
	{
		ImGui::Begin("Properties");

		CHEngine::SceneObject* selected = m_Scene.FindByID(m_SelectedObjectID);
		if (selected)
		{
			char nameBuf[256];
			std::strncpy(nameBuf, selected->Name.c_str(), sizeof(nameBuf));
			nameBuf[sizeof(nameBuf) - 1] = '\0';
			if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf)))
				selected->Name = nameBuf;

			ImGui::Separator();
			ImGui::SeparatorText("Transform");
			ImGui::DragFloat3("Position", glm::value_ptr(selected->ObjectTransform.Position), 0.05f);
			ImGui::DragFloat3("Rotation", glm::value_ptr(selected->ObjectTransform.Rotation), 0.5f);
			ImGui::DragFloat3("Scale",    glm::value_ptr(selected->ObjectTransform.Scale),    0.01f, 0.01f, 100.0f);
			if (ImGui::Button("Reset Transform")) selected->ObjectTransform = {};

			ImGui::Separator();
			ImGui::SeparatorText("Material");
			ImGui::ColorEdit4("Color",   glm::value_ptr(selected->Color));
			ImGui::Checkbox  ("Visible", &selected->Visible);

			ImGui::Separator();
			ImGui::SeparatorText("Info");
			uint32_t tv = 0, ti = 0;
			for (auto& m : selected->Meshes)
			{
				tv += static_cast<uint32_t>(m.GetVertices().size());
				ti += static_cast<uint32_t>(m.GetIndices().size());
			}
			ImGui::Text("Meshes: %zu",    selected->Meshes.size());
			ImGui::Text("Vertices: %u",   tv);
			ImGui::Text("Triangles: %u",  ti / 3);

			if (ImGui::Button("Focus Camera (F)"))
				FocusOnSelected();
		}
		else
		{
			ImGui::TextDisabled("No object selected");
		}

		ImGui::End();
	}

	void DrawCameraPanel()
	{
		ImGui::Begin("Camera");

		// --- View presets ---
		ImGui::SeparatorText("View Presets");
		if (ImGui::Button("Persp"))   { SetViewPreset(-90.0f, -15.0f); m_Camera.SetFOV(45.0f); }
		ImGui::SameLine();
		if (ImGui::Button("Front"))   SetViewPreset(-90.0f,   0.0f);
		ImGui::SameLine();
		if (ImGui::Button("Back"))    SetViewPreset( 90.0f,   0.0f);
		ImGui::SameLine();
		if (ImGui::Button("Top"))     SetViewPreset(-90.0f, -89.0f);
		ImGui::SameLine();
		if (ImGui::Button("Right"))   SetViewPreset(180.0f,   0.0f);
		ImGui::SameLine();
		if (ImGui::Button("Left"))    SetViewPreset(  0.0f,   0.0f);

		ImGui::Spacing();

		// --- Orbit controls ---
		ImGui::SeparatorText("Orbit");

		bool orbitChanged = false;
		orbitChanged |= ImGui::DragFloat3("Target",   glm::value_ptr(m_OrbitTarget), 0.05f);
		orbitChanged |= ImGui::DragFloat ("Distance", &m_OrbitDist, 0.1f, 0.3f, 500.0f, "%.2f");

		float yaw   = m_Camera.GetYaw();
		float pitch = m_Camera.GetPitch();
		float fov   = m_Camera.GetFOV();
		if (ImGui::SliderFloat("Yaw",   &yaw,    -180.0f, 180.0f, "%.1f")) { m_Camera.SetYaw(yaw);     orbitChanged = true; }
		if (ImGui::SliderFloat("Pitch", &pitch,   -89.0f,  89.0f, "%.1f")) { m_Camera.SetPitch(pitch); orbitChanged = true; }
		if (ImGui::SliderFloat("FOV",   &fov,      10.0f, 120.0f, "%.1f"))   m_Camera.SetFOV(fov);

		if (orbitChanged) ApplyOrbit();

		glm::vec3 pos = m_Camera.GetPosition();
		ImGui::Text("Camera: %.2f  %.2f  %.2f", pos.x, pos.y, pos.z);

		ImGui::Spacing();

		// --- Follow & reset ---
		ImGui::Checkbox("Follow Selected", &m_FollowObject);
		ImGui::SameLine();
		if (ImGui::Button("Reset"))
		{
			m_OrbitTarget = { 0.0f, 0.0f, 0.0f };
			m_OrbitDist   = 8.0f;
			m_Camera.SetYaw(-90.0f);
			m_Camera.SetPitch(-15.0f);
			m_Camera.SetFOV(45.0f);
			ApplyOrbit();
		}

		ImGui::End();
	}

	void DrawGizmo()
	{
		CHEngine::SceneObject* selected = m_Scene.FindByID(m_SelectedObjectID);
		if (!selected) return;

		ImGuizmo::SetOrthographic(false);
		ImGuiIO& io = ImGui::GetIO();
		ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

		glm::mat4 view = m_Camera.GetViewMatrix();
		glm::mat4 proj = m_Camera.GetProjectionMatrix(m_AspectRatio);

		float t[3] = { selected->ObjectTransform.Position.x, selected->ObjectTransform.Position.y, selected->ObjectTransform.Position.z };
		float r[3] = { selected->ObjectTransform.Rotation.x,  selected->ObjectTransform.Rotation.y,  selected->ObjectTransform.Rotation.z  };
		float s[3] = { selected->ObjectTransform.Scale.x,     selected->ObjectTransform.Scale.y,     selected->ObjectTransform.Scale.z     };

		float mm[16];
		ImGuizmo::RecomposeMatrixFromComponents(t, r, s, mm);

		ImGuizmo::Manipulate(
			glm::value_ptr(view), glm::value_ptr(proj),
			m_GizmoOperation, m_GizmoMode, mm);

		if (ImGuizmo::IsUsing())
		{
			ImGuizmo::DecomposeMatrixToComponents(mm, t, r, s);
			selected->ObjectTransform.Position = { t[0], t[1], t[2] };
			selected->ObjectTransform.Rotation = { r[0], r[1], r[2] };
			selected->ObjectTransform.Scale    = { s[0], s[1], s[2] };
		}
	}

	void ImportModel(const std::string& filepath)
	{
		auto& res = CHEngine::Application::Get().GetRenderResources();
		auto result = CHEngine::ModelLoader::Load(filepath, res);
		if (result.success)
		{
			auto* obj = m_Scene.AddModel(result.name, std::move(result.meshes));
			m_SelectedObjectID = obj->ID;
			FocusOnSelected(); // automatically frame the imported model
		}
	}

	// =========================================================================
	// State
	// =========================================================================
	CHEngine::Scene             m_Scene;
	CHEngine::Camera            m_Camera;

	CHEngine::ShaderHandle      m_MeshShader;
	CHEngine::ShaderHandle      m_GridShader;
	CHEngine::VertexArrayHandle m_GridVAO;
	CHEngine::RenderAPIHandle   m_RenderApi;

	// Orbit camera
	glm::vec3 m_OrbitTarget = { 0.0f, 0.0f, 0.0f };
	float     m_OrbitDist   = 8.0f;
	bool      m_FollowObject = false;

	uint32_t m_SelectedObjectID = 0;
	float    m_AspectRatio      = 16.0f / 9.0f;

	ImGuizmo::OPERATION m_GizmoOperation = ImGuizmo::TRANSLATE;
	ImGuizmo::MODE      m_GizmoMode      = ImGuizmo::WORLD;
	bool                m_LocalMode      = false;
	bool                m_ShowGrid       = true;

	std::chrono::steady_clock::time_point m_LastTime;
};
