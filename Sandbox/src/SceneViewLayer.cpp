#include "SceneViewLayer.h"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cstring>

// ============================================================================
//  Constructor
// ============================================================================

SceneViewLayer::SceneViewLayer()
    : Layer("SceneView")
    , m_Camera(45.0f, 0.1f, 500.0f)
    , m_LastTime(std::chrono::steady_clock::now())
{
    UIActive::SetTheme(AppTheme::RetroOS);
    UIActive::SyncLayout();

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
    ApplyOrbit();
}

// ============================================================================
//  Layer overrides
// ============================================================================

void SceneViewLayer::OnUpdate()
{
    auto  now = std::chrono::steady_clock::now();
    float dt  = std::chrono::duration<float>(now - m_LastTime).count();
    m_LastTime = now;
    (void)dt;

    RenderScene();
}

void SceneViewLayer::OnImGuiRender()
{
    // ── Screen dimensions ──────────────────────────────────────────────────
    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    const float  W    = disp.x;
    const float  H    = disp.y;
    if (H > 0.0f) m_AspectRatio = W / H;

    ImGuizmo::BeginFrame();
    UpdateCameraInput();
    UIActive::SyncLayout();

    // ── Layout computation ─────────────────────────────────────────────────
    //
    //  ┌─────────────────────────────────────────────────────────────┐
    //  │  TOOLBAR  (top, full width, locked)                         │
    //  ├──────────┬──────────────────────────────┬───────────────────┤
    //  │  SCENE   │                              │  PROPERTIES       │
    //  │          │       3D  VIEWPORT           │                   │
    //  │  objects │       (OpenGL bg)            │  Transform        │
    //  │  list    │                              │  Material / Info  │
    //  │          │                              ├───────────────────┤
    //  │  + Import│                              │  CAMERA           │
    //  │          │                              │  View / Orbit     │
    //  └──────────┴──────────────────────────────┴───────────────────┘
    //
    //  To add a panel:
    //    1. Compute pos/size from W, H and g_Layout fields
    //    2. Add a LayoutConfig field if needed
    //    3. Call DrawNewPanel(pos, size, reset)
    // ──────────────────────────────────────────────────────────────────────

    const auto& L      = UIActive::g_Layout;
    const float leftW  = W * L.leftFrac;
    const float rightW = W * L.rightFrac;
    const float sH     = H - L.toolbarH;

    const bool reset = m_ResetLayout;
    m_ResetLayout = false;

    const ImVec2 tbPos    = { 0.0f,        0.0f                          };
    const ImVec2 tbSize   = { W,            L.toolbarH                    };

    const ImVec2 scenePos = { 0.0f,         L.toolbarH                    };
    const ImVec2 sceneSize= { leftW,         sH                            };

    const ImVec2 propsPos = { W - rightW,    L.toolbarH                    };
    const ImVec2 propsSize= { rightW,         sH * L.propsFrac              };

    const ImVec2 camPos   = { W - rightW,    L.toolbarH + sH * L.propsFrac };
    const ImVec2 camSize  = { rightW,         sH * (1.0f - L.propsFrac)    };

    DrawToolbar      (tbPos,    tbSize);
    DrawScenePanel   (scenePos, sceneSize, reset);
    DrawPropsPanel   (propsPos, propsSize, reset);
    DrawCameraPanel  (camPos,   camSize,   reset);
    DrawOrbitIndicator();
    DrawGizmo();
}

// ============================================================================
//  Orbit camera
// ============================================================================

void SceneViewLayer::ApplyOrbit()
{
    glm::vec3 fwd = m_Camera.GetForward();
    m_Camera.SetPosition(m_OrbitTarget - fwd * m_OrbitDist);
}

void SceneViewLayer::SetViewPreset(float yaw, float pitch)
{
    m_Camera.SetYaw(yaw);
    m_Camera.SetPitch(pitch);
    ApplyOrbit();
}

void SceneViewLayer::FocusOnSelected()
{
    CHEngine::SceneObject* obj = m_Scene.FindByID(m_SelectedObjectID);
    if (!obj) return;

    m_OrbitTarget = obj->ObjectTransform.Position;

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

// ============================================================================
//  Camera input
// ============================================================================

void SceneViewLayer::UpdateCameraInput()
{
    ImGuiIO& io = ImGui::GetIO();

    // Only when cursor is over the 3D viewport (not any panel)
    if (io.WantCaptureMouse) return;

    // Two-finger swipe: zoom (Y) + orbit yaw (X)
    if (io.MouseWheel != 0.0f || io.MouseWheelH != 0.0f)
    {
        if (io.KeyCtrl)
        {
            // Ctrl + vertical swipe → orbit pitch
            m_Camera.SetPitch(m_Camera.GetPitch() + io.MouseWheel * 3.0f);
            ApplyOrbit();
        }
        else
        {
            if (io.MouseWheel != 0.0f)
            {
                float factor = 1.0f - io.MouseWheel * 0.12f;
                m_OrbitDist  = glm::clamp(m_OrbitDist * factor, 0.3f, 500.0f);
            }
            if (io.MouseWheelH != 0.0f)
                m_Camera.SetYaw(m_Camera.GetYaw() - io.MouseWheelH * 3.0f);

            ApplyOrbit();
        }
    }

    // RMB drag → pan (grab mode)
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Right, 1.0f))
    {
        float     panScale = m_OrbitDist * 0.0015f;
        glm::vec3 right    = m_Camera.GetRight();
        glm::vec3 up       = m_Camera.GetUp();
        m_OrbitTarget -= right * io.MouseDelta.x * panScale;
        m_OrbitTarget += up    * io.MouseDelta.y * panScale;
        ApplyOrbit();
    }

    // MMB drag → pan (mouse with middle button)
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f))
    {
        float     panScale = m_OrbitDist * 0.0015f;
        glm::vec3 right    = m_Camera.GetRight();
        glm::vec3 up       = m_Camera.GetUp();
        m_OrbitTarget -= right * io.MouseDelta.x * panScale;
        m_OrbitTarget += up    * io.MouseDelta.y * panScale;
        ApplyOrbit();
    }

    // F → frame selected
    if (ImGui::IsKeyPressed(ImGuiKey_F) && m_SelectedObjectID != 0)
        FocusOnSelected();

    // Follow mode
    if (m_FollowObject && m_SelectedObjectID != 0)
    {
        auto* obj = m_Scene.FindByID(m_SelectedObjectID);
        if (obj) { m_OrbitTarget = obj->ObjectTransform.Position; ApplyOrbit(); }
    }
}

// ============================================================================
//  Grid / axes geometry
// ============================================================================

void SceneViewLayer::BuildGrid()
{
    auto& res = CHEngine::Application::Get().GetRenderResources();

    struct LineVertex { glm::vec3 pos, color; };
    std::vector<LineVertex> verts;
    std::vector<uint32_t>   indices;

    const int       N       = 10;
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
        flat.push_back(v.pos.x);    flat.push_back(v.pos.y);    flat.push_back(v.pos.z);
        flat.push_back(v.color.r);  flat.push_back(v.color.g);  flat.push_back(v.color.b);
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

// ============================================================================
//  Rendering
// ============================================================================

void SceneViewLayer::RenderScene()
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

        float t[3] = { obj->ObjectTransform.Position.x,
                        obj->ObjectTransform.Position.y,
                        obj->ObjectTransform.Position.z };
        float r[3] = { obj->ObjectTransform.Rotation.x,
                        obj->ObjectTransform.Rotation.y,
                        obj->ObjectTransform.Rotation.z };
        float s[3] = { obj->ObjectTransform.Scale.x,
                        obj->ObjectTransform.Scale.y,
                        obj->ObjectTransform.Scale.z };

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
            bool hasTex = mesh.DiffuseTexture.IsValid();
            shader->SetInt(CHEngine::String("u_UseTexture"), hasTex ? 1 : 0);
            if (hasTex)
            {
                auto* tex = res.Get(mesh.DiffuseTexture);
                if (tex) tex->Bind(0);
                shader->SetInt(CHEngine::String("u_DiffuseTexture"), 0);
            }

            auto* vao = res.Get(mesh.GetVertexArray());
            if (vao) api->DrawIndexed(vao);

            if (hasTex)
            {
                auto* tex = res.Get(mesh.DiffuseTexture);
                if (tex) tex->Unbind();
            }
        }
    }
}

// ============================================================================
//  UI — Toolbar
// ============================================================================

void SceneViewLayer::DrawToolbar(ImVec2 pos, ImVec2 size)
{
    UIActive::BeginToolbar(pos, size);

    // Vertical-centering helper — nudges each item so its visual centre
    // aligns with the toolbar's horizontal midline.
    {
        const float winH    = ImGui::GetWindowHeight();
        const float padY    = 9.0f;
        const float startY  = ImGui::GetCursorPosY();
        const float centerY = startY + (winH - padY * 2.0f) * 0.5f;

        struct VC {
            float cy;
            void operator()(float itemH) const {
                ImGui::SetCursorPosY(cy - itemH * 0.5f);
            }
        } vcenter{ centerY };

        // Hotkeys T / R / S / Undo
        if (!ImGui::GetIO().WantTextInput)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_T, false)) m_GizmoOperation = ImGuizmo::TRANSLATE;
            if (ImGui::IsKeyPressed(ImGuiKey_R, false)) m_GizmoOperation = ImGuizmo::ROTATE;
            if (ImGui::IsKeyPressed(ImGuiKey_S, false)) m_GizmoOperation = ImGuizmo::SCALE;

            // Cmd+Z (macOS) или Alt+Z (Windows/Linux)
            const bool undoMod = ImGui::GetIO().KeySuper   // Cmd на Mac
                              || ImGui::GetIO().KeyAlt;    // Alt на Windows
            if (undoMod && ImGui::IsKeyPressed(ImGuiKey_Z, false))
                m_UndoStack.Undo();
        }

        // Gizmo segmented control
        const char* gizmoLabels[] = { "Translate", "Rotate", "Scale" };
        int gizmoSel = (m_GizmoOperation == ImGuizmo::TRANSLATE) ? 0
                     : (m_GizmoOperation == ImGuizmo::ROTATE   ) ? 1 : 2;

        vcenter(ImGui::GetFrameHeight());
        if (UIActive::SegmentedControl("##gizmo", gizmoLabels, 3, &gizmoSel,
                                       ImVec2(272.0f, 0.0f)))
        {
            m_GizmoOperation = (gizmoSel == 0) ? ImGuizmo::TRANSLATE
                             : (gizmoSel == 1) ? ImGuizmo::ROTATE
                                               : ImGuizmo::SCALE;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("T  Translate\nR  Rotate\nS  Scale");

        ImGui::SameLine(0, 20);
        vcenter(20.0f);
        if (UIActive::Toggle("Local", &m_LocalMode))
            m_GizmoMode = m_LocalMode ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

        ImGui::SameLine(0, 20);
        vcenter(20.0f);
        UIActive::Toggle("Grid", &m_ShowGrid);

        // Snap hint
        {
            bool   shiftHeld = ImGui::GetIO().KeyShift;
            ImVec4 col = shiftHeld
                ? ImVec4(0.20f, 0.60f, 1.00f, 1.0f)
                : ImVec4(0.43f, 0.43f, 0.45f, 1.0f);
            ImGui::SameLine(0, 20);
            vcenter(ImGui::GetTextLineHeight());
            ImGui::PushStyleColor(ImGuiCol_Text, col);
            ImGui::TextUnformatted(shiftHeld ? "\xe2\x87\xa7 Snap ON" : "\xe2\x87\xa7 Snap");
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Hold Shift to snap:\n"
                                  "  Translate  1 unit\n"
                                  "  Rotate     45\xc2\xb0\n"
                                  "  Scale      0.1 unit");
        }

        // Right side: Theme toggle + FPS
        float       fps    = ImGui::GetIO().Framerate;
        char        fpsBuf[32];
        snprintf(fpsBuf, sizeof(fpsBuf), "%.0f fps", fps);
        const char* themeLabel = (UIActive::g_Theme == AppTheme::RetroOS) ? "Theme: Retro" : "Theme: Dark";
        const float pad = ImGui::GetStyle().FramePadding.x;
        float rightBlockW = ImGui::CalcTextSize(themeLabel).x + pad * 2.0f
                          + 8.0f
                          + ImGui::CalcTextSize(fpsBuf).x
                          + ImGui::GetStyle().WindowPadding.x;

        float startX = ImGui::GetWindowWidth() - rightBlockW;
        if (startX > ImGui::GetCursorPosX() + 10.0f)
        {
            ImGui::SetCursorPosX(startX);
            vcenter(ImGui::GetFrameHeight());
            if (ImGui::Button(themeLabel))
            {
                AppTheme next = (UIActive::g_Theme == AppTheme::RetroOS)
                              ? AppTheme::Dark : AppTheme::RetroOS;
                UIActive::SetTheme(next);
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Switch UI theme");
            ImGui::SameLine(0, 8);
            vcenter(ImGui::GetTextLineHeight());
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f,0.557f,0.576f,1.0f));
            ImGui::TextUnformatted(fpsBuf);
            ImGui::PopStyleColor();
        }

    } // end vcenter scope

    UIActive::EndToolbar();
}

// ============================================================================
//  UI — Scene Hierarchy
// ============================================================================

void SceneViewLayer::DrawScenePanel(ImVec2 pos, ImVec2 size, bool resetSize)
{
    UIActive::BeginPanel("Scene", pos, size, 0, resetSize);

    if (UIActive::PrimaryButton("+ Import Model", ImVec2(-1.0f, 0.0f)))
    {
        std::string path = CHEngine::FileDialog::OpenFile(
            "3D Models (*.obj, *.glb, *.gltf)", "");
        if (!path.empty()) ImportModel(path);
    }

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator,
        ImVec4(0.282f, 0.282f, 0.290f, 0.45f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    uint32_t deleteID = 0;
    for (auto& obj : m_Scene.GetObjects())
    {
        bool isSelected = (obj->ID == m_SelectedObjectID);
        ImGuiTreeNodeFlags flags =
            ImGuiTreeNodeFlags_Leaf         |
            ImGuiTreeNodeFlags_SpanAvailWidth|
            ImGuiTreeNodeFlags_FramePadding;
        if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

        bool opened = ImGui::TreeNodeEx(
            (void*)(intptr_t)obj->ID, flags, "  %s", obj->Name.c_str());

        if (ImGui::IsItemClicked())
            m_SelectedObjectID = obj->ID;

        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Focus (F)")) FocusOnSelected();
            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Text,
                ImVec4(1.0f, 0.231f, 0.188f, 1.0f));
            if (ImGui::MenuItem("Delete")) deleteID = obj->ID;
            ImGui::PopStyleColor();
            ImGui::EndPopup();
        }
        if (opened) ImGui::TreePop();
    }

    if (m_Scene.GetObjects().empty())
    {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.686f,0.686f,0.706f,1.0f));
        ImGui::TextWrapped("No objects in scene.\nImport a model to begin.");
        ImGui::PopStyleColor();
    }

    if (deleteID > 0)
    {
        if (m_SelectedObjectID == deleteID) m_SelectedObjectID = 0;
        m_Scene.RemoveObject(deleteID);
    }

    UIActive::EndPanel();
}

// ============================================================================
//  UI — Properties / Inspector
// ============================================================================

void SceneViewLayer::DrawPropsPanel(ImVec2 pos, ImVec2 size, bool resetSize)
{
    UIActive::BeginPanel("Properties", pos, size, 0, resetSize);

    CHEngine::SceneObject* sel = m_Scene.FindByID(m_SelectedObjectID);
    if (!sel)
    {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.686f,0.686f,0.706f,1.0f));
        ImGui::TextUnformatted("No object selected");
        ImGui::PopStyleColor();
        UIActive::EndPanel();
        return;
    }

    // Name field
    char nameBuf[256];
    std::strncpy(nameBuf, sel->Name.c_str(), sizeof(nameBuf));
    nameBuf[sizeof(nameBuf) - 1] = '\0';
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputText("##name", nameBuf, sizeof(nameBuf)))
        sel->Name = nameBuf;

    // TRANSFORM
    UIActive::SectionHeader("TRANSFORM");

    const float labelW = 64.0f;
    // Для undo: захватить трансформ при начале редактирования слайдера
    auto row = [&](const char* label, const char* id, glm::vec3& vec,
                   float speed, float mn = -FLT_MAX, float mx = FLT_MAX)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f,0.557f,0.576f,1.0f));
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();
        ImGui::SameLine(labelW);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::DragFloat3(id, glm::value_ptr(vec), speed, mn, mx);
        if (ImGui::IsItemActivated())
            m_TransformBeforeDrag = sel->ObjectTransform;
        if (ImGui::IsItemDeactivatedAfterEdit())
            m_UndoStack.PushTransform(&m_Scene, sel->ID, m_TransformBeforeDrag);
    };
    row("Position", "##pos", sel->ObjectTransform.Position, 0.05f);
    row("Rotation", "##rot", sel->ObjectTransform.Rotation, 0.5f);
    row("Scale",    "##scl", sel->ObjectTransform.Scale,    0.01f, 0.001f, 1000.0f);

    ImGui::Spacing();
    if (ImGui::Button("Reset Transform", ImVec2(-1.0f, 0.0f)))
    {
        m_UndoStack.PushTransform(&m_Scene, sel->ID, sel->ObjectTransform);
        sel->ObjectTransform = {};
    }

    // MATERIAL
    UIActive::SectionHeader("MATERIAL");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::ColorEdit4("##color", glm::value_ptr(sel->Color));
    ImGui::Spacing();
    {
        bool before = sel->Visible;
        if (UIActive::Toggle("Visible", &sel->Visible) && sel->Visible != before)
            m_UndoStack.PushVisibility(&m_Scene, sel->ID, before);
    }

    // INFO
    UIActive::SectionHeader("INFO");
    uint32_t tv = 0, ti = 0;
    for (auto& m : sel->Meshes)
    {
        tv += static_cast<uint32_t>(m.GetVertices().size());
        ti += static_cast<uint32_t>(m.GetIndices().size());
    }
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f,0.557f,0.576f,1.0f));
    ImGui::Text("Meshes      %zu",  sel->Meshes.size());
    ImGui::Text("Vertices    %u",   tv);
    ImGui::Text("Triangles   %u",   ti / 3);
    ImGui::PopStyleColor();

    ImGui::Spacing();
    if (ImGui::Button("Focus Camera  (F)", ImVec2(-1.0f, 0.0f)))
        FocusOnSelected();

    UIActive::EndPanel();
}

// ============================================================================
//  UI — Camera settings
// ============================================================================

void SceneViewLayer::DrawCameraPanel(ImVec2 pos, ImVec2 size, bool resetSize)
{
    UIActive::BeginPanel("Camera", pos, size, 0, resetSize);

    UIActive::SectionHeader("VIEW");

    if (UIActive::PrimaryButton("Perspective", ImVec2(-1.0f, 0.0f)))
        { SetViewPreset(-90.0f, -15.0f); m_Camera.SetFOV(45.0f); }

    ImGui::Spacing();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
    float hw = (ImGui::GetContentRegionAvail().x - 4.0f) * 0.5f;

    struct ViewPreset { const char* name; float yaw; float pitch; };
    ViewPreset presets[] = {
        { "Front",  -90.0f,   0.0f }, { "Back",    90.0f,   0.0f },
        { "Top",    -90.0f, -89.9f }, { "Bottom", -90.0f,  89.9f },
        { "Right",  180.0f,   0.0f }, { "Left",     0.0f,   0.0f },
    };
    for (int i = 0; i < 6; i++)
    {
        if (i % 2 != 0) ImGui::SameLine();
        if (ImGui::Button(presets[i].name, ImVec2(hw, 0)))
            SetViewPreset(presets[i].yaw, presets[i].pitch);
    }
    ImGui::PopStyleVar();

    UIActive::SectionHeader("ORBIT");

    bool        orb = false;
    const float lw  = 62.0f;

    auto sliderRow = [&](const char* label, const char* id,
                          float* val, float mn, float mx, const char* fmt)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f,0.557f,0.576f,1.0f));
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();
        ImGui::SameLine(lw);
        ImGui::SetNextItemWidth(-1.0f);
        return ImGui::SliderFloat(id, val, mn, mx, fmt);
    };
    auto dragRow = [&](const char* label, const char* id,
                        float* val, float spd, float mn, float mx)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f,0.557f,0.576f,1.0f));
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();
        ImGui::SameLine(lw);
        ImGui::SetNextItemWidth(-1.0f);
        return ImGui::DragFloat(id, val, spd, mn, mx, "%.2f");
    };

    float yaw   = m_Camera.GetYaw();
    float pitch = m_Camera.GetPitch();
    float fov   = m_Camera.GetFOV();

    if (sliderRow("Yaw",   "##yaw",  &yaw,   -180.0f, 180.0f, "%.1f°"))
        { m_Camera.SetYaw(yaw);     orb = true; }
    if (sliderRow("Pitch", "##pitch",&pitch,   -89.0f,  89.0f, "%.1f°"))
        { m_Camera.SetPitch(pitch); orb = true; }
    if (sliderRow("FOV",   "##fov",  &fov,     10.0f,  120.0f, "%.1f°"))
        m_Camera.SetFOV(fov);
    if (dragRow  ("Dist",  "##dist", &m_OrbitDist, 0.1f, 0.3f, 500.0f))
        orb = true;

    if (orb) ApplyOrbit();

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f,0.557f,0.576f,1.0f));
    ImGui::TextUnformatted("Target");
    ImGui::PopStyleColor();
    ImGui::SameLine(lw);
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::DragFloat3("##target", glm::value_ptr(m_OrbitTarget), 0.05f))
        ApplyOrbit();

    glm::vec3 cpos = m_Camera.GetPosition();
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.357f,0.357f,0.376f,1.0f));
    ImGui::Text("Pos  %.2f  %.2f  %.2f", cpos.x, cpos.y, cpos.z);
    ImGui::PopStyleColor();

    UIActive::SectionHeader("CONTROLS");
    UIActive::Toggle("Follow Selected", &m_FollowObject);

    ImGui::Spacing();
    if (UIActive::DestructiveButton("Reset Camera", ImVec2(-1.0f, 0.0f)))
    {
        m_OrbitTarget = { 0.0f, 0.0f, 0.0f };
        m_OrbitDist   = 8.0f;
        m_Camera.SetYaw(-90.0f);
        m_Camera.SetPitch(-15.0f);
        m_Camera.SetFOV(45.0f);
        ApplyOrbit();
    }

    UIActive::EndPanel();
}

// ============================================================================
//  UI — Orbit indicator
// ============================================================================

void SceneViewLayer::DrawOrbitIndicator()
{
    ImGuiIO&    io = ImGui::GetIO();
    const float W  = io.DisplaySize.x;
    const float H  = io.DisplaySize.y;

    glm::mat4 view = m_Camera.GetViewMatrix();
    glm::mat4 proj = m_Camera.GetProjectionMatrix(m_AspectRatio);

    glm::vec4 clip = proj * view * glm::vec4(m_OrbitTarget, 1.0f);
    if (clip.w <= 0.0f) return;

    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    if (glm::abs(ndc.x) > 1.0f || glm::abs(ndc.y) > 1.0f) return;

    float sx = (ndc.x + 1.0f) * 0.5f * W;
    float sy = (1.0f - ndc.y) * 0.5f * H;

    ImDrawList*  dl  = ImGui::GetBackgroundDrawList();
    const float  r   = 6.0f;
    const float  gap = 2.5f;
    const ImU32  col = IM_COL32(255, 255, 255, 140);
    const ImU32  out = IM_COL32(0,   0,   0,    80);

    dl->AddLine({sx-r-1,sy},   {sx-gap-1,sy},   out, 1.5f);
    dl->AddLine({sx+gap-1,sy}, {sx+r-1,sy},     out, 1.5f);
    dl->AddLine({sx,sy-r-1},   {sx,sy-gap-1},   out, 1.5f);
    dl->AddLine({sx,sy+gap-1}, {sx,sy+r-1},     out, 1.5f);

    dl->AddLine({sx-r,sy},   {sx-gap,sy},   col, 1.5f);
    dl->AddLine({sx+gap,sy}, {sx+r,sy},     col, 1.5f);
    dl->AddLine({sx,sy-r},   {sx,sy-gap},   col, 1.5f);
    dl->AddLine({sx,sy+gap}, {sx,sy+r},     col, 1.5f);

    dl->AddCircleFilled({sx, sy}, 1.8f, col);
}

// ============================================================================
//  UI — Transform gizmo
// ============================================================================

void SceneViewLayer::DrawGizmo()
{
    CHEngine::SceneObject* selected = m_Scene.FindByID(m_SelectedObjectID);
    if (!selected) return;

    ImGuizmo::SetOrthographic(false);
    ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

    glm::mat4 view = m_Camera.GetViewMatrix();
    glm::mat4 proj = m_Camera.GetProjectionMatrix(m_AspectRatio);

    float t[3] = { selected->ObjectTransform.Position.x,
                    selected->ObjectTransform.Position.y,
                    selected->ObjectTransform.Position.z };
    float r[3] = { selected->ObjectTransform.Rotation.x,
                    selected->ObjectTransform.Rotation.y,
                    selected->ObjectTransform.Rotation.z };
    float s[3] = { selected->ObjectTransform.Scale.x,
                    selected->ObjectTransform.Scale.y,
                    selected->ObjectTransform.Scale.z };

    float mm[16];
    ImGuizmo::RecomposeMatrixFromComponents(t, r, s, mm);

    float snapT[3] = { 1.0f,  1.0f,  1.0f  };
    float snapR[3] = { 45.0f, 45.0f, 45.0f };
    float snapS[3] = { 0.1f,  0.1f,  0.1f  };
    float* snap = nullptr;
    if (io.KeyShift)
    {
        if      (m_GizmoOperation == ImGuizmo::TRANSLATE) snap = snapT;
        else if (m_GizmoOperation == ImGuizmo::ROTATE)    snap = snapR;
        else if (m_GizmoOperation == ImGuizmo::SCALE)     snap = snapS;
    }

    // Запомнить трансформ перед началом drag
    if (!m_GizmoWasUsing && ImGuizmo::IsUsing())
        m_TransformBeforeDrag = selected->ObjectTransform;

    ImGuizmo::Manipulate(
        glm::value_ptr(view), glm::value_ptr(proj),
        m_GizmoOperation, m_GizmoMode, mm,
        nullptr, snap);

    if (ImGuizmo::IsUsing())
    {
        ImGuizmo::DecomposeMatrixToComponents(mm, t, r, s);
        selected->ObjectTransform.Position = { t[0], t[1], t[2] };
        selected->ObjectTransform.Rotation = { r[0], r[1], r[2] };
        selected->ObjectTransform.Scale    = { s[0], s[1], s[2] };
    }

    // Drag закончился — пушим команду
    if (m_GizmoWasUsing && !ImGuizmo::IsUsing())
        m_UndoStack.PushTransform(&m_Scene, selected->ID, m_TransformBeforeDrag);

    m_GizmoWasUsing = ImGuizmo::IsUsing();
}

// ============================================================================
//  Model import
// ============================================================================

void SceneViewLayer::ImportModel(const std::string& filepath)
{
    auto& res    = CHEngine::Application::Get().GetRenderResources();
    auto  result = CHEngine::ModelLoader::Load(filepath, res);
    if (!result.success)
        return;

    // Compute geometric centroid across all meshes so the gizmo
    // snaps to the visual centre of the model.
    glm::vec3 centroid(0.0f);
    size_t    totalVerts = 0;
    for (auto& mesh : result.meshes)
    {
        for (const auto& v : mesh.GetVertices())
        {
            centroid += v.Position;
            ++totalVerts;
        }
    }
    if (totalVerts > 0) centroid /= static_cast<float>(totalVerts);

    // Re-centre vertices around local origin and rebuild GPU buffers.
    if (totalVerts > 0 && glm::length(centroid) > 1e-5f)
    {
        for (auto& mesh : result.meshes)
        {
            res.DestroyVertexArray(mesh.GetVertexArray());
            auto verts = mesh.GetVertices();
            for (auto& v : verts) v.Position -= centroid;
            mesh.Build(res, verts, mesh.GetIndices());
        }
    }

    auto* obj = m_Scene.AddModel(result.name, std::move(result.meshes));
    obj->ObjectTransform.Position = centroid;
    m_SelectedObjectID = obj->ID;
    m_UndoStack.PushImport(&m_Scene, obj->ID, &m_SelectedObjectID);
    FocusOnSelected();
}
