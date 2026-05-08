#include "EditorViewport.h"

#include <CHEngine/Application.h>
#include <CHEngine/Render/RenderFacade.h>
#include <Render/UniformBlocks.h>
#include <Render/IRenderFactory.h>
#include <Render/Descriptors.h>

#include <CHEngine/Camera/EditorCamera.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/matrix.hpp>
#include <cstring>

namespace Sandbox {

EditorViewport::EditorViewport()
{
    m_MeshShader = CHEngine::RenderFacade::CreateShaderFromFile(
        CHEngine::String("Mesh"),
        CHEngine::String("shaders/mesh.slang"));
    CHEngine::RenderFacade::SetDefaultMeshShader(m_MeshShader);

    m_GridShader = CHEngine::RenderFacade::CreateShaderFromFile(
        CHEngine::String("Grid"),
        CHEngine::String("shaders/grid.slang"));

    BuildGrid();
}

void EditorViewport::Begin()
{
    ImGuizmo::BeginFrame();
}

void EditorViewport::End()
{
}

void EditorViewport::BeginSceneRender(SceneSession* scene_session)
{
    CHE_PROFILE_FUNCTION();
    if (!scene_session)
        return;

    if (!scene_session->ViewportCamera)
        scene_session->ViewportCamera = CHEngine::MakeScope<CHEngine::EditorCamera>();

    auto* viewport_camera = scene_session->ViewportCamera.get();
    if (!viewport_camera)
        return;

    {
        ImVec2 fbScale = ImGui::GetIO().DisplayFramebufferScale;
        const uint32_t vpW = static_cast<uint32_t>(m_ViewportSize.x * fbScale.x);
        const uint32_t vpH = static_cast<uint32_t>(m_ViewportSize.y * fbScale.y);
        if (vpW > 0 && vpH > 0)
            CHEngine::RenderFacade::SetViewportSize(vpW, vpH);
    }

    if (scene_session->SessionState != SceneSession::State::Play)
    {
        glm::mat4 vp    = viewport_camera->GetViewProjection();
        glm::mat4 invVP = glm::inverse(vp);
        glm::vec3 camPos = viewport_camera->GetPosition();

        CHEngine::UBOCamera cameraUBO{};
        std::memcpy(cameraUBO.ViewProjection, glm::value_ptr(vp),    sizeof(cameraUBO.ViewProjection));
        std::memcpy(cameraUBO.InvViewProj,    glm::value_ptr(invVP), sizeof(cameraUBO.InvViewProj));
        cameraUBO.CameraPos[0] = camPos.x;
        cameraUBO.CameraPos[1] = camPos.y;
        cameraUBO.CameraPos[2] = camPos.z;
        cameraUBO.CameraPos[3] = 0.0f;

        CHEngine::RenderFacade::SetSceneCamera(cameraUBO);

        // Register grid as a pre-tonemap callback so it renders into HDR with depth testing.
        // The callback is called by RenderSystem after MainColorPass, before TonemapPass.
        if (m_ShowGrid && scene_session->SessionState == SceneSession::State::Edit
            && m_GridVB.IsValid() && m_GridIB.IsValid() && m_GridPipeline.IsValid())
        {
            CHEngine::RenderFacade::SetPreTonemapCallback([this, scene_session]() {
                this->RegisterEditorPasses(scene_session);
            });
        }
        else
        {
            CHEngine::RenderFacade::ClearPreTonemapCallback();
        }

        // Update grid camera UBO so the grid shader knows InvViewProj
        if (m_GridCameraUBO.IsValid()) {
            if (auto* f = CHEngine::RenderFacade::GetRenderFactory()) {
                f->UpdateBuffer(m_GridCameraUBO,
                    std::span<const std::byte>(
                        reinterpret_cast<const std::byte*>(&cameraUBO), sizeof(cameraUBO)));
            }
        }
    }
}

void EditorViewport::RegisterEditorPasses(SceneSession* scene_session)
{
    // Guards checked by caller (BeginSceneRender callback)

    // GridPass: alpha-blend grid into the HDR target (before tonemap),
    // with depth testing so scene objects occlude the grid.
    CHEngine::TextureHandle hdrTarget   = CHEngine::RenderFacade::GetViewportHDRTexture();
    CHEngine::TextureHandle depthTarget = CHEngine::RenderFacade::GetViewportDepthTexture();
    if (!hdrTarget.IsValid()) return;

    // We need the Camera UBO — get it via a helper in RenderFacade
    // For now, register a simple pass that uses the existing camera UBO from RenderSystem.
    // The grid shader reads camera.InvViewProj to reconstruct world rays.

    CHEngine::PassDesc gridPass;
    gridPass.Name          = "GridPass";
    gridPass.Pipeline      = m_GridPipeline;
    gridPass.ColorLoadOp   = CHEngine::ELoadOp::Load;   // preserve LDR scene content
    gridPass.ColorStoreOp  = CHEngine::EStoreOp::Store;
    // Use physical pixel size (with Retina scale) matching the LDR texture resolution.
    gridPass.ViewportWidth  = CHEngine::RenderFacade::GetViewportWidth();
    gridPass.ViewportHeight = CHEngine::RenderFacade::GetViewportHeight();

    // Render into HDR (before tonemap) with depth testing.
    gridPass.ColorAttachments.push_back(hdrTarget);
    gridPass.Reads.push_back(hdrTarget);
    gridPass.Writes.push_back(hdrTarget);

    // Attach depth for depth testing (Load = preserve from MainColorPass).
    if (depthTarget.IsValid()) {
        gridPass.DepthAttachment = depthTarget;
        gridPass.DepthLoadOp = CHEngine::ELoadOp::Load;
        gridPass.Reads.push_back(depthTarget);
    }

    // Bind camera UBO at slot 0 (grid shader: ConstantBuffer<CameraUBO> camera)
    if (m_GridCameraUBO.IsValid()) {
        gridPass.Uniforms.push_back({
            m_GridCameraUBO,
            static_cast<uint32_t>(CHEngine::EUniformBlock::Camera),
            0, sizeof(CHEngine::UBOCamera)
        });
    }

    // Draw the fullscreen NDC quad
    CHEngine::DrawDesc draw;
    draw.VertexBuffer  = m_GridVB;
    draw.IndexBuffer   = m_GridIB;
    draw.IdxFormat     = CHEngine::IndexFormat::UInt32;
    draw.IndexCount    = m_GridIndexCount;
    draw.InstanceCount = 1;
    gridPass.Draws.push_back(std::move(draw));

    CHEngine::RenderFacade::GetFrameGraph().AddPass(std::move(gridPass));
}

void EditorViewport::EndSceneRender()
{
    CHE_PROFILE_FUNCTION();
}

void EditorViewport::DrawImGui(GizmoSystem& gizmo,
                               EditorCameraController& camera_controller,
                               SceneSession* scene_session,
                               const ImVec2& vp_pos,
                               const ImVec2& vp_size,
                               ImGuizmo::OPERATION gizmo_operation,
                               ImGuizmo::MODE gizmo_mode)
{
    if (!scene_session)
        return;

    ImGui::SetNextWindowPos(vp_pos);
    ImGui::SetNextWindowSize(vp_size);
    // Zero padding so Image fills the entire window and GetCursorScreenPos == window top-left
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("##viewport", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
                     | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus
                     | ImGuiWindowFlags_NoNavFocus);
    ImGui::PopStyleVar();

    m_ViewportHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    // Use cursor screen pos (not window pos) — window has padding that offsets the image.
    // ImGuizmo::SetRect must match exactly where the Image is drawn.
    m_ViewportPos = ImGui::GetCursorScreenPos();

    ImVec2 panelSize = ImGui::GetContentRegionAvail();

    if (panelSize.x > 1.0f && panelSize.y > 1.0f
        && (panelSize.x != m_ViewportSize.x || panelSize.y != m_ViewportSize.y))
    {
        m_ViewportSize = panelSize;

        ImVec2 fbScale = ImGui::GetIO().DisplayFramebufferScale;
        const uint32_t newW = static_cast<uint32_t>(panelSize.x * fbScale.x);
        const uint32_t newH = static_cast<uint32_t>(panelSize.y * fbScale.y);
        CHEngine::RenderFacade::SetViewportSize(newW, newH);

        camera_controller.SetAspectRatio(panelSize.x / panelSize.y);
        scene_session->ViewportSize = { panelSize.x, panelSize.y };
        if (!scene_session->ViewportCamera)
            scene_session->ViewportCamera = CHEngine::MakeScope<CHEngine::EditorCamera>();
        scene_session->ViewportCamera->SetViewportSize(panelSize.x, panelSize.y);
    }

    // Display viewport output texture in ImGui.
    const uint64_t colorTexID = CHEngine::RenderFacade::GetViewportColorTexID();
    if (colorTexID != 0)
    {
        static bool s_LoggedOnce = false;
        if (!s_LoggedOnce) {
            CHE_CORE_INFO("EditorViewport: displaying LDR texture 0x{:x} ({}x{})",
                          colorTexID, (int)panelSize.x, (int)panelSize.y);
            s_LoggedOnce = true;
        }
        const bool isMetal = (CHEngine::Application::Get().GetRenderAPIType() == CHEngine::ERenderAPI::METAL);
        ImVec2 uv0 = isMetal ? ImVec2(0, 0) : ImVec2(0, 1);
        ImVec2 uv1 = isMetal ? ImVec2(1, 1) : ImVec2(1, 0);
        ImGui::Image(static_cast<ImTextureID>(colorTexID), panelSize, uv0, uv1);
    }

    gizmo.Draw(scene_session, gizmo_operation, gizmo_mode, m_ViewportPos, m_ViewportSize);

    if (scene_session->SessionState != SceneSession::State::Edit)
    {
        const ImVec4 col = (scene_session->SessionState == SceneSession::State::Play)
            ? ImVec4(0.20f, 0.75f, 0.20f, 0.85f)
            : ImVec4(0.90f, 0.70f, 0.10f, 0.85f);
        const float thick = 3.0f;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 wMin = ImGui::GetWindowPos();
        ImVec2 wMax = { wMin.x + ImGui::GetWindowWidth(), wMin.y + ImGui::GetWindowHeight() };
        dl->AddRect(wMin, wMax, ImGui::ColorConvertFloat4ToU32(col), 0.0f, 0, thick);
    }

    ImGui::End();
}

void EditorViewport::BuildGrid()
{
    CHEngine::IRenderFactory* f = CHEngine::RenderFacade::GetRenderFactory();
    if (!f) return;

    // Full-screen NDC quad — the grid shader reconstructs world position via InvViewProj.
    float verts[] = { -1.f, -1.f,  1.f, -1.f,  1.f, 1.f,  -1.f, 1.f };
    uint32_t idx[] = { 0, 1, 2,  0, 2, 3 };

    std::span<const std::byte> vbBytes{reinterpret_cast<const std::byte*>(verts), sizeof(verts)};
    m_GridVB = f->CreateBuffer(sizeof(verts), CHEngine::BufferUsage::Vertex,
                                CHEngine::MemoryType::GpuOnly, vbBytes, CHEngine::String("grid_vb"));

    std::span<const std::byte> ibBytes{reinterpret_cast<const std::byte*>(idx), sizeof(idx)};
    m_GridIB = f->CreateBuffer(sizeof(idx), CHEngine::BufferUsage::Index,
                                CHEngine::MemoryType::GpuOnly, ibBytes, CHEngine::String("grid_ib"));
    m_GridIndexCount = 6;

    // Camera UBO for grid shader
    m_GridCameraUBO = f->CreateBuffer(sizeof(CHEngine::UBOCamera),
                                       CHEngine::BufferUsage::Uniform,
                                       CHEngine::MemoryType::CpuToGpu, {},
                                       CHEngine::String("grid_camera_ubo"));

    // Create grid pipeline — blending ON so transparent areas let scene show through.
    if (m_GridShader.IsValid()) {
        CHEngine::PipelineDesc desc;
        desc.Shader        = m_GridShader;
        desc.VertexLayout  = CHEngine::VertexInputLayout(
            { CHEngine::VertexAttributeDesc(CHEngine::VertexFormat::Float2, 0, 0) }, 8u);
        desc.Primitive       = CHEngine::PrimitiveType::Triangles;
        // Depth test ON so objects occlude grid; depth write OFF so grid doesn't block objects
        desc.Depth.Test      = true;
        desc.Depth.Write     = false;
        desc.Depth.Compare   = CHEngine::CompareOp::LessEqual;
        desc.Raster.Cull     = CHEngine::CullMode::None;
        desc.Blend.Enable  = true;
        desc.Blend.SrcColor = CHEngine::BlendFactor::SrcAlpha;
        desc.Blend.DstColor = CHEngine::BlendFactor::OneMinusSrcAlpha;
        desc.Blend.ColorOp  = CHEngine::BlendOp::Add;
        desc.Blend.SrcAlpha = CHEngine::BlendFactor::One;
        desc.Blend.DstAlpha = CHEngine::BlendFactor::OneMinusSrcAlpha;
        desc.Blend.AlphaOp  = CHEngine::BlendOp::Add;
        desc.ColorFormats.push_back(CHEngine::TextureFormat::RGBA16_FLOAT);  // HDR target
        desc.DepthFormat = CHEngine::TextureFormat::D24_UNORM_S8_UINT;
        m_GridPipeline = f->CreatePipeline(std::move(desc));
    }
}

} // namespace Sandbox
