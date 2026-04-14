#include "SceneViewLayer.h"

#include <CHEngine/Render/RenderFacade.h>
#include <CHEngine/Scene/Entity.h>
#include <CHEngine/World/ISystem.h>
#include <CHEngine/World/WorldEvents.h>

// ============================================================================
//  Constructor
// ============================================================================

SceneViewLayer::SceneViewLayer()
    : Layer("SceneView")
    , m_World(&m_Scene)
    , m_Camera(45.0f, 0.1f, 500.0f)
{
    UIActive::SetTheme(AppTheme::RetroOS);
    UIActive::SyncLayout();

    m_MeshShader = CHEngine::RenderFacade::CreateShaderFromFile(
        CHEngine::String("Mesh"),
        CHEngine::String("shaders/mesh.vert"),
        CHEngine::String("shaders/mesh.frag")
    );
    CHEngine::RenderFacade::SetDefaultMeshShader(m_MeshShader);
    m_GridShader = CHEngine::RenderFacade::CreateShaderFromFile(
        CHEngine::String("Grid"),
        CHEngine::String("shaders/grid.vert"),
        CHEngine::String("shaders/grid.frag")
    );

    BuildGrid();
    m_Framebuffer = CHEngine::RenderFacade::CreateFramebuffer(1280, 720);
    m_Camera.SetPitch(-30.0f);   // default: look slightly down so grid is visible
    ApplyOrbit();

    m_RecentFiles.LoadFromFile("recent_scenes.txt");

    // Восстанавливаем сцену если был рестарт при смене API
    TryRestoreSession();
}

void SceneViewLayer::FlushSimulationEvents()
{
    const bool wasSimulating = m_World.IsSimulating();
    m_World.SetSimulating(true);
    m_World.Update(CHEngine::Timestep(0.0f));
    m_World.SetSimulating(wasSimulating);
}

// ============================================================================
//  Layer overrides
// ============================================================================

void SceneViewLayer::OnUpdate(CHEngine::Timestep dt)
{
    CHE_PROFILE_FUNCTION();
    SyncEditorCameraToECS();
    RenderScene(dt);
}

void SceneViewLayer::SyncEditorCameraToECS()
{
    if (!m_Scene.IsEntityHandleValid(m_EditorCameraEntity))
    {
        m_EditorCameraEntity = m_Scene.CreateEntity("EditorCamera");
        if (auto* entity = m_Scene.TryGetEntity(m_EditorCameraEntity))
        {
            if (!entity->HasComponent<CHEngine::CameraComponent>())
                entity->AddComponent<CHEngine::CameraComponent>(CHEngine::CameraComponent{});
        }
    }

    auto* entity = m_Scene.TryGetEntity(m_EditorCameraEntity);
    if (!entity
        || !entity->HasComponent<CHEngine::CameraComponent>()
        || !entity->HasComponent<CHEngine::TransformComponent>())
        return;
    auto& cameraComp = entity->GetComponent<CHEngine::CameraComponent>();
    auto& transformComp = entity->GetComponent<CHEngine::TransformComponent>();

    cameraComp.FOV = m_Camera.GetFOV();
    cameraComp.NearClip = 0.1f;
    cameraComp.FarClip = 500.0f;
    cameraComp.AspectRatio = m_AspectRatio;
    cameraComp.Active = true;
    cameraComp.Primary = true;

    CHEngine::Transform cameraTransform = cameraComp.CameraTransform;
    cameraTransform.Position = m_Camera.GetPosition();
    cameraTransform.Rotation.x = m_Camera.GetPitch();
    cameraTransform.Rotation.y = m_Camera.GetYaw();
    cameraTransform.Rotation.z = 0.0f;
    cameraTransform.Scale = { 1.0f, 1.0f, 1.0f };
    cameraComp.CameraTransform = cameraTransform;
    transformComp.ObjectTransform = cameraTransform;
}

void SceneViewLayer::OnEvent(CHEngine::Event& e)
{
    m_World.OnEvent(e);
}

void SceneViewLayer::OnImGuiRender()
{
    CHE_PROFILE_FUNCTION();
    // ── Screen dimensions ──────────────────────────────────────────────────
    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    const float  W    = disp.x;
    const float  H    = disp.y;

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

    // ── Center viewport panel ─────────────────────────────────────────────
    {
        const float  vpH    = sH - m_ContentBrowserHeight;
        const ImVec2 vpPos  = { leftW,       L.toolbarH };
        const ImVec2 vpSize = { W - leftW - rightW, vpH };

        ImGui::SetNextWindowPos(vpPos);
        ImGui::SetNextWindowSize(vpSize);
        ImGui::Begin("##viewport", nullptr,
            ImGuiWindowFlags_NoTitleBar        |
            ImGuiWindowFlags_NoScrollbar       |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoMove            |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus);

        // AllowWhenBlockedByActiveItem: хост остаётся "hovered" пока
        // пользователь тянет Image-виджет (drag), что нужно для вращения камеры
        m_ViewportHovered = ImGui::IsWindowHovered(
            ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        m_ViewportPos     = ImGui::GetWindowPos();

        ImVec2 panelSize = ImGui::GetContentRegionAvail();

        // Resize FBO if panel size changed.
        // FBO создаётся в пикселях (с учётом Retina): panelSize × fbScale.
        // ImGui::Image вызывается с panelSize в screen points — ImGui сам
        // масштабирует UV при рендере, поэтому картинка не пикселится.
        bool resizedThisFrame = false;
        if (panelSize.x > 1.0f && panelSize.y > 1.0f &&
            (panelSize.x != m_ViewportSize.x || panelSize.y != m_ViewportSize.y))
        {
            m_ViewportSize = panelSize;
            ImVec2 fbScale = ImGui::GetIO().DisplayFramebufferScale;
            auto* fbo = CHEngine::RenderFacade::GetFramebuffer(m_Framebuffer);
            if (fbo) fbo->Resize(
                static_cast<uint32_t>(panelSize.x * fbScale.x),
                static_cast<uint32_t>(panelSize.y * fbScale.y));
            m_AspectRatio = panelSize.x / panelSize.y;
            resizedThisFrame = true;
        }

        // Display the rendered scene as a texture (flip Y: OpenGL bottom-left → ImGui top-left).
        // Skip display on resize frame: Metal FBO has uninitialized Private texture,
        // cleared on next Bind() via MTLLoadActionClear. Prevents pink garbage flash.
        auto* fbo2 = CHEngine::RenderFacade::GetFramebuffer(m_Framebuffer);
        if (fbo2 && !resizedThisFrame)
        {
            void* nativeTex = fbo2->GetNativeColorAttachment();
            // OpenGL: flip Y (bottom-left origin). Metal: no flip (top-left origin).
            const bool isMetal = (CHEngine::Application::Get().GetRenderAPIType()
                                  == CHEngine::ERenderAPI::METAL);
            ImVec2 uv0 = isMetal ? ImVec2(0, 0) : ImVec2(0, 1);
            ImVec2 uv1 = isMetal ? ImVec2(1, 1) : ImVec2(1, 0);
            ImGui::Image((ImTextureID)nativeTex, panelSize, uv0, uv1);
        }

        // Gizmo MUST be inside the viewport window and use its drawlist
        DrawGizmo();

        // ── Цветная рамка режима воспроизведения ──────────────────────────
        if (m_EditorState != EditorState::Edit)
        {
            const ImVec4 col = (m_EditorState == EditorState::Play)
                ? ImVec4(0.20f, 0.75f, 0.20f, 0.85f)   // зелёная — Play
                : ImVec4(0.90f, 0.70f, 0.10f, 0.85f);  // жёлтая  — Pause
            const float  thick = 3.0f;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 wMin = ImGui::GetWindowPos();
            ImVec2 wMax = { wMin.x + ImGui::GetWindowWidth(),
                            wMin.y + ImGui::GetWindowHeight() };
            dl->AddRect(wMin, wMax, ImGui::ColorConvertFloat4ToU32(col), 0.0f, 0, thick);
        }
        // ─────────────────────────────────────────────────────────────────

        ImGui::End();
    }
    // ─────────────────────────────────────────────────────────────────────

    // ── Content Browser — bottom panel ────────────────────────────────────
    {
        const float  bottomY     = L.toolbarH + (sH - m_ContentBrowserHeight);
        const ImVec2 browserPos  = { leftW,               bottomY                    };
        const ImVec2 browserSize = { W - leftW - rightW,  m_ContentBrowserHeight     };
        std::string action = m_ContentBrowser.OnImGuiRender(browserPos, browserSize);
        if (!action.empty()) {
            if (action.rfind("model:", 0) == 0)
                ImportModel(action.substr(6));
            else if (action.rfind("scene:", 0) == 0)
                LoadScene(action.substr(6));
        }
    }
    // ─────────────────────────────────────────────────────────────────────

    DrawProfilerPanel();
    DrawOrbitIndicator();
}
