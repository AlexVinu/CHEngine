#include "SceneViewLayer.h"


// ============================================================================
//  Constructor
// ============================================================================

SceneViewLayer::SceneViewLayer()
    : Layer("SceneView")
    , m_Resources(CHEngine::Application::Get().GetRenderResources())
    , m_Camera(45.0f, 0.1f, 500.0f)
{
    UIActive::SetTheme(AppTheme::RetroOS);
    UIActive::SyncLayout();

    auto& app = CHEngine::Application::Get();
    m_RenderApi = app.GetRenderApiHandle();

    m_MeshShader = m_Resources.CreateShaderFromFile(
        CHEngine::String("Mesh"),
        CHEngine::String("shaders/mesh.vert"),
        CHEngine::String("shaders/mesh.frag")
    );
    m_GridShader = m_Resources.CreateShaderFromFile(
        CHEngine::String("Grid"),
        CHEngine::String("shaders/grid.vert"),
        CHEngine::String("shaders/grid.frag")
    );

    BuildGrid();
    m_Framebuffer = m_Resources.CreateFramebuffer(1280, 720);
    m_Camera.SetPitch(-30.0f);   // default: look slightly down so grid is visible
    ApplyOrbit();

    m_RecentFiles.LoadFromFile("recent_scenes.txt");

    // Восстанавливаем сцену если был рестарт при смене API
    TryRestoreSession();
}

// ============================================================================
//  Layer overrides
// ============================================================================

void SceneViewLayer::OnUpdate(float /*dt*/)
{
    RenderScene();
}

void SceneViewLayer::OnImGuiRender()
{
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
        if (panelSize.x > 1.0f && panelSize.y > 1.0f &&
            (panelSize.x != m_ViewportSize.x || panelSize.y != m_ViewportSize.y))
        {
            m_ViewportSize = panelSize;
            ImVec2 fbScale = ImGui::GetIO().DisplayFramebufferScale;
            auto* fbo = m_Resources.Get(m_Framebuffer);
            if (fbo) fbo->Resize(
                static_cast<uint32_t>(panelSize.x * fbScale.x),
                static_cast<uint32_t>(panelSize.y * fbScale.y));
            m_AspectRatio = panelSize.x / panelSize.y;
        }

        // Display the rendered scene as a texture (flip Y: OpenGL bottom-left → ImGui top-left)
        auto* fbo2 = m_Resources.Get(m_Framebuffer);
        if (fbo2)
        {
            void* nativeTex = fbo2->GetNativeColorAttachment();
            // OpenGL: flip Y (bottom-left origin). Metal: no flip (top-left origin).
            const bool isMetal = (CHEngine::Application::Get().GetRenderAPIType()
                                  == CHEngine::ERenderAPI::METALL);
            ImVec2 uv0 = isMetal ? ImVec2(0, 0) : ImVec2(0, 1);
            ImVec2 uv1 = isMetal ? ImVec2(1, 1) : ImVec2(1, 0);
            ImGui::Image((ImTextureID)nativeTex, panelSize, uv0, uv1);
        }

        // Gizmo MUST be inside the viewport window and use its drawlist
        DrawGizmo();

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

    DrawOrbitIndicator();
}
