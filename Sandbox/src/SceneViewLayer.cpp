#include "SceneViewLayer.h"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

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
    m_Framebuffer = res.CreateFramebuffer(1280, 720);
    m_Camera.SetPitch(-30.0f);   // default: look slightly down so grid is visible
    ApplyOrbit();

    m_RecentFiles.LoadFromFile("recent_scenes.txt");
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

        // Resize FBO if panel size changed
        if (panelSize.x > 1.0f && panelSize.y > 1.0f &&
            (panelSize.x != m_ViewportSize.x || panelSize.y != m_ViewportSize.y))
        {
            m_ViewportSize = panelSize;
            auto& res2 = CHEngine::Application::Get().GetRenderResources();
            auto* fbo2 = res2.Get(m_Framebuffer);
            if (fbo2) fbo2->Resize((uint32_t)panelSize.x, (uint32_t)panelSize.y);
            m_AspectRatio = panelSize.x / panelSize.y;
        }

        // Display the rendered scene as a texture (flip Y: OpenGL bottom-left → ImGui top-left)
        auto& res2 = CHEngine::Application::Get().GetRenderResources();
        auto* fbo2 = res2.Get(m_Framebuffer);
        if (fbo2)
        {
            uint32_t texID = fbo2->GetColorAttachmentID();
            ImGui::Image((ImTextureID)(uintptr_t)texID, panelSize, ImVec2(0, 1), ImVec2(1, 0));
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
