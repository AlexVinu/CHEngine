#include "EditorViewport.h"

#include <CHEngine/Application.h>
#include <CHEngine/Render/RenderFacade.h>
#include <CHEngine/ResourceManager/ResourceManager.h>
#include <Render/UniformBlocks.h>
#include <Render/IRenderFactory.h>
#include <Render/Descriptors.h>

#include <CHEngine/Camera/EditorCamera.h>
#include <CHEngine/Scene/Components.h>
#include <CHEngine/Scene/Scene.h>
#include <CHEngine/Scene/Entity.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/matrix.hpp>
#include <cstring>

namespace Sandbox {

EditorViewport::EditorViewport()
{
    m_MeshShader = CHEngine::ResourceManager::Instance().Load<CHEngine::ShaderHandle>(
        std::string("Mesh"), std::string("shaders/mesh.slang"));
    CHEngine::RenderFacade::SetDefaultMeshShader(m_MeshShader);

    m_GridShader = CHEngine::ResourceManager::Instance().Load<CHEngine::ShaderHandle>(
        std::string("Grid"), std::string("shaders/grid.slang"));

    m_SphereShader = CHEngine::ResourceManager::Instance().Load<CHEngine::ShaderHandle>(
        std::string("Sphere"), std::string("shaders/sphere.slang"));

    m_PBRShader = CHEngine::ResourceManager::Instance().Load<CHEngine::ShaderHandle>(
        std::string("PBR"), std::string("shaders/pbr.slang"));

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

        // Register grid as a PRE-SCENE callback: grid is drawn FIRST (clears HDR),
        // then MainColorPass draws objects ON TOP of the grid.
        if (m_ShowGrid && scene_session->SessionState == SceneSession::State::Edit
            && m_GridVB.IsValid() && m_GridIB.IsValid() && m_GridPipeline.IsValid())
        {
            CHEngine::RenderFacade::SetPreSceneCallback([this, scene_session]() {
                this->RegisterEditorPasses(scene_session);
            });
        }
        else
        {
            CHEngine::RenderFacade::ClearPreSceneCallback();
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
    else
    {
        // Play mode: editor grid must not render.
        CHEngine::RenderFacade::ClearPreSceneCallback();
    }
}

void EditorViewport::RegisterEditorPasses(SceneSession* scene_session)
{
    // Guards checked by caller (BeginSceneRender callback)

    // GridPass: drawn FIRST — clears HDR to background color, draws grid.
    // MainColorPass (called after) loads this content and draws objects on top.
    // Objects naturally cover the grid since they render AFTER it.
    CHEngine::TextureHandle hdrTarget   = CHEngine::RenderFacade::GetViewportHDRTexture();
    CHEngine::TextureHandle depthTarget = CHEngine::RenderFacade::GetViewportDepthTexture();
    if (!hdrTarget.IsValid()) return;

    // We need the Camera UBO — get it via a helper in RenderFacade
    // For now, register a simple pass that uses the existing camera UBO from RenderSystem.
    // The grid shader reads camera.InvViewProj to reconstruct world rays.

    CHEngine::PassDesc gridPass;
    gridPass.Name          = "GridPass";
    gridPass.Pipeline      = m_GridPipeline;
    // Clear HDR to background color — grid is the first thing drawn.
    gridPass.ColorLoadOp   = CHEngine::ELoadOp::Clear;
    gridPass.ColorStoreOp  = CHEngine::EStoreOp::Store;
    gridPass.ClearColor    = { 0.0f, 0.0f, 0.0f, 0.0f };  // editor background
    gridPass.ViewportWidth  = CHEngine::RenderFacade::GetViewportWidth();
    gridPass.ViewportHeight = CHEngine::RenderFacade::GetViewportHeight();

    // Render into HDR as the background layer.
    gridPass.ColorAttachments.push_back(hdrTarget);
    gridPass.Writes.push_back(hdrTarget);

    // Clear depth so MainColorPass starts with a fresh depth buffer.
    if (depthTarget.IsValid()) {
        gridPass.DepthAttachment = depthTarget;
        gridPass.DepthLoadOp  = CHEngine::ELoadOp::Clear;
        gridPass.ClearDepth   = 1.0f;
        gridPass.Writes.push_back(depthTarget);
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

// ─────────────────────────────────────────────────────────────────────────────
// Вспомогательные функции визуализации камеры (ImGui overlay)
// ─────────────────────────────────────────────────────────────────────────────
namespace {

// World → viewport pixel coordinates. Returns false if behind camera or far outside.
bool WorldToViewport(const glm::vec3& worldPos,
                     const glm::mat4& editorVP,
                     const ImVec2& vpPos,
                     const ImVec2& vpSize,
                     ImVec2& outScreen)
{
    glm::vec4 clip = editorVP * glm::vec4(worldPos, 1.0f);
    if (clip.w < 0.001f) return false;

    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    if (ndc.x < -1.5f || ndc.x > 1.5f ||
        ndc.y < -1.5f || ndc.y > 1.5f) return false;

    outScreen.x = vpPos.x + (ndc.x * 0.5f + 0.5f) * vpSize.x;
    outScreen.y = vpPos.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * vpSize.y;
    return true;
}

void DrawWorldLine(ImDrawList* dl,
                   const glm::vec3& a, const glm::vec3& b,
                   const glm::mat4& editorVP,
                   const ImVec2& vpPos, const ImVec2& vpSize,
                   ImU32 color, float thickness = 1.5f)
{
    ImVec2 sa, sb;
    if (WorldToViewport(a, editorVP, vpPos, vpSize, sa) &&
        WorldToViewport(b, editorVP, vpPos, vpSize, sb))
    {
        dl->AddLine(sa, sb, color, thickness);
    }
}

void DrawCameraFrustum(ImDrawList* dl,
                       const CHEngine::Transform& transform,
                       const CHEngine::SceneCamera& cam,
                       const glm::mat4& editorVP,
                       const ImVec2& vpPos, const ImVec2& vpSize,
                       bool isSelected)
{
    const ImU32 colorFrustum  = isSelected
        ? IM_COL32(255, 220,  50, 220)
        : IM_COL32(180, 220, 255, 160);
    const ImU32 colorBody     = IM_COL32(255, 255, 255, 200);
    const float lineW         = isSelected ? 2.0f : 1.2f;

    glm::mat4 rot = glm::eulerAngleYXZ(
        glm::radians(transform.Rotation.y),
        glm::radians(transform.Rotation.x),
        glm::radians(transform.Rotation.z));

    glm::vec3 camPos   = transform.Position;
    glm::vec3 forward  = glm::vec3(rot * glm::vec4(0, 0, -1, 0));
    glm::vec3 right2   = glm::vec3(rot * glm::vec4(1,  0,  0, 0));
    glm::vec3 up2      = glm::vec3(rot * glm::vec4(0,  1,  0, 0));

    float fov    = cam.GetPerspectiveVerticalFOV();
    float aspect = cam.GetProjectionType() ==
                   CHEngine::SceneCamera::ProjectionType::Perspective
                   ? 16.0f / 9.0f : 1.0f;
    float nearD  = cam.GetPerspectiveNearClip();
    float farD   = std::min(cam.GetPerspectiveFarClip(), 8.0f);

    auto frustumCorners = [&](float d, float halfH) -> std::array<glm::vec3, 4>
    {
        float halfW = halfH * aspect;
        glm::vec3 center = camPos + forward * d;
        return {{
            center - right2 * halfW - up2 * halfH,
            center + right2 * halfW - up2 * halfH,
            center + right2 * halfW + up2 * halfH,
            center - right2 * halfW + up2 * halfH
        }};
    };

    float halfHNear = nearD * std::tan(fov * 0.5f);
    float halfHFar  = farD  * std::tan(fov * 0.5f);

    auto nearCorners = frustumCorners(nearD, halfHNear);
    auto farCorners  = frustumCorners(farD,  halfHFar);

    glm::vec3 wPts[8];
    for (int i = 0; i < 4; ++i) wPts[i]     = nearCorners[i];
    for (int i = 0; i < 4; ++i) wPts[i + 4] = farCorners[i];

    for (int i = 0; i < 4; ++i)
        DrawWorldLine(dl, wPts[i], wPts[(i + 1) % 4], editorVP, vpPos, vpSize, colorFrustum, lineW);
    for (int i = 0; i < 4; ++i)
        DrawWorldLine(dl, wPts[i + 4], wPts[((i + 1) % 4) + 4], editorVP, vpPos, vpSize, colorFrustum, lineW);
    for (int i = 0; i < 4; ++i)
        DrawWorldLine(dl, wPts[i], wPts[i + 4], editorVP, vpPos, vpSize, colorFrustum, lineW);

    auto rotVec = [&](glm::vec3 v) -> glm::vec3 {
        return glm::vec3(rot * glm::vec4(v, 0.0f));
    };

    const float bW = 0.18f, bH = 0.12f, bD = 0.24f;

    glm::vec3 right   = rotVec({ 1, 0, 0 });
    glm::vec3 up      = rotVec({ 0, 1, 0 });
    glm::vec3 fwd     = rotVec({ 0, 0, -1 });

    glm::vec3 box[8];
    for (int xi = -1; xi <= 1; xi += 2)
    for (int yi = -1; yi <= 1; yi += 2)
    for (int zi =  0; zi <= 1; zi += 1)
    {
        int idx = ((xi + 1) / 2) * 4 + ((yi + 1) / 2) * 2 + zi;
        box[idx] = camPos + right * ((float)xi * bW) + up * ((float)yi * bH) + fwd * (zi == 0 ? bD * 0.5f : -bD * 0.5f);
    }

    int edges[12][2] = {
        {0,2},{2,6},{6,4},{4,0},
        {1,3},{3,7},{7,5},{5,1},
        {0,1},{2,3},{4,5},{6,7}
    };
    for (auto& e : edges)
        DrawWorldLine(dl, box[e[0]], box[e[1]], editorVP, vpPos, vpSize, colorBody, 1.0f);

    glm::vec3 tvBase1 = camPos + right * (bW) + up * (bH) + fwd * (bD * 0.45f);
    glm::vec3 tvBase2 = camPos + right * (-bW) + up * (bH) + fwd * (bD * 0.45f);
    glm::vec3 tvTip   = camPos + up * (bH * 2.2f) + fwd * (bD * 0.2f);
    DrawWorldLine(dl, tvBase1, tvTip,   editorVP, vpPos, vpSize, colorBody, 1.0f);
    DrawWorldLine(dl, tvBase2, tvTip,   editorVP, vpPos, vpSize, colorBody, 1.0f);
    DrawWorldLine(dl, tvBase1, tvBase2, editorVP, vpPos, vpSize, colorBody, 1.0f);
}

} // anonymous namespace

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

    // ── Визуализация камер (только в Edit-режиме) ─────────────────────────────
    if (scene_session->SessionState == SceneSession::State::Edit
        && scene_session->ViewportCamera
        && scene_session->EditorScene)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        glm::mat4 editorVP = scene_session->ViewportCamera->GetViewProjection();

        scene_session->EditorScene->ForEach<CHEngine::CameraComponent, CHEngine::TransformComponent>(
            [&](CHEngine::EntityHandle handle, const CHEngine::UUID&,
                CHEngine::CameraComponent& camComp,
                CHEngine::TransformComponent& tc)
            {
                bool selected = (handle == scene_session->SelectedEntity);
                DrawCameraFrustum(dl, tc.ObjectTransform, camComp.Camera,
                                  editorVP, m_ViewportPos, m_ViewportSize, selected);
            });
    }

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
        desc.Primitive   = CHEngine::PrimitiveType::Triangles;
        // Grid drawn first as background — no depth test needed, objects rendered after
        desc.Depth.Test  = false;
        desc.Depth.Write = false;
        desc.Raster.Cull = CHEngine::CullMode::None;
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
