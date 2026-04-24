#include "EditorViewport.h"

#include <CHEngine/Application.h>
#include <CHEngine/Render/RenderFacade.h>
#include <Render/UniformBlocks.h>

#include <CHEngine/Camera/EditorCamera.h>
#include <glm/gtc/type_ptr.hpp>
#include <cstring>

namespace Sandbox {

EditorViewport::EditorViewport()
{
    m_MeshShader = CHEngine::RenderFacade::CreateShaderFromFile(
        CHEngine::String("Mesh"),
        CHEngine::String("shaders/mesh.vert"),
        CHEngine::String("shaders/mesh.frag"));
    CHEngine::RenderFacade::SetDefaultMeshShader(m_MeshShader);

    m_GridShader = CHEngine::RenderFacade::CreateShaderFromFile(
        CHEngine::String("Grid"),
        CHEngine::String("shaders/grid.vert"),
        CHEngine::String("shaders/grid.frag"));

    BuildGrid();
    m_Framebuffer = CHEngine::RenderFacade::CreateFramebuffer(1280, 720);
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

    auto* fbo = CHEngine::RenderFacade::GetFramebuffer(m_Framebuffer);
    if (fbo)
        fbo->Bind();
    CHEngine::RenderFacade::Clear();

    // During Play, scene/RenderSystem owns the camera UBO; do not apply editor orbit camera here.
    if (scene_session->SessionState != SceneSession::State::Play)
    {
        glm::mat4 vp = viewport_camera->GetViewProjection();
        glm::mat4 invVP = glm::inverse(vp);
        glm::vec3 camPos = viewport_camera->GetPosition();

        CHEngine::UBOCamera cameraUBO{};
        std::memcpy(cameraUBO.ViewProjection, glm::value_ptr(vp), sizeof(cameraUBO.ViewProjection));
        std::memcpy(cameraUBO.InvViewProj, glm::value_ptr(invVP), sizeof(cameraUBO.InvViewProj));
        cameraUBO.CameraPos[0] = camPos.x;
        cameraUBO.CameraPos[1] = camPos.y;
        cameraUBO.CameraPos[2] = camPos.z;
        cameraUBO.CameraPos[3] = 0.0f;

        CHEngine::RenderFacade::SetSceneCamera(cameraUBO);
    }

}

void EditorViewport::DrawEditorOverlays(SceneSession* scene_session)
{
    CHE_PROFILE_FUNCTION();
    if (!scene_session)
        return;

    // Editor-only floor grid; hide during Play/Pause so the runtime view matches a game camera.
    const bool showEditorGrid = m_ShowGrid
        && (scene_session->SessionState == SceneSession::State::Edit);
    if (!showEditorGrid)
        return;

    if (m_GridShader.IsValid() && m_GridVAO.IsValid())
    {
        CHEngine::RenderFacade::SetBlend(true);
        CHEngine::RenderFacade::SetDepthWrite(false);

        CHEngine::RenderFacade::Submit(m_GridShader, m_GridVAO, glm::mat4(1.0f));

        CHEngine::RenderFacade::SetDepthWrite(true);
        CHEngine::RenderFacade::SetBlend(false);
    }
}

void EditorViewport::EndSceneRender()
{
    CHE_PROFILE_FUNCTION();
    auto* fbo = CHEngine::RenderFacade::GetFramebuffer(m_Framebuffer);
    if (fbo)
        fbo->Unbind();
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
    ImGui::Begin("##viewport", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
                     | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus
                     | ImGuiWindowFlags_NoNavFocus);

    m_ViewportHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    m_ViewportPos = ImGui::GetWindowPos();

    ImVec2 panelSize = ImGui::GetContentRegionAvail();

    bool resizedThisFrame = false;
    if (panelSize.x > 1.0f && panelSize.y > 1.0f
        && (panelSize.x != m_ViewportSize.x || panelSize.y != m_ViewportSize.y))
    {
        m_ViewportSize = panelSize;
        ImVec2 fbScale = ImGui::GetIO().DisplayFramebufferScale;
        auto* fbo = CHEngine::RenderFacade::GetFramebuffer(m_Framebuffer);
        if (fbo)
            fbo->Resize(static_cast<uint32_t>(panelSize.x * fbScale.x),
                        static_cast<uint32_t>(panelSize.y * fbScale.y));
        camera_controller.SetAspectRatio(panelSize.x / panelSize.y);
        scene_session->ViewportSize = { panelSize.x, panelSize.y };
        if (!scene_session->ViewportCamera)
            scene_session->ViewportCamera = CHEngine::MakeScope<CHEngine::EditorCamera>();
        scene_session->ViewportCamera->SetViewportSize(panelSize.x, panelSize.y);
        resizedThisFrame = true;
    }

    auto* fbo2 = CHEngine::RenderFacade::GetFramebuffer(m_Framebuffer);
    if (fbo2 && !resizedThisFrame)
    {
        void* nativeTex = fbo2->GetNativeColorAttachment();
        const bool isMetal = (CHEngine::Application::Get().GetRenderAPIType() == CHEngine::ERenderAPI::METAL);
        ImVec2 uv0 = isMetal ? ImVec2(0, 0) : ImVec2(0, 1);
        ImVec2 uv1 = isMetal ? ImVec2(1, 1) : ImVec2(1, 0);
        ImGui::Image((ImTextureID)nativeTex, panelSize, uv0, uv1);
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
    float verts[] = {
        -1.0f, -1.0f,
        1.0f, -1.0f,
        1.0f, 1.0f,
        -1.0f, 1.0f,
    };
    uint32_t indices[] = { 0, 1, 2, 0, 2, 3 };

    m_GridVAO = CHEngine::RenderFacade::CreateVertexArray();
    auto* vao = CHEngine::RenderFacade::GetVertexArray(m_GridVAO);
    if (!vao)
        return;

    auto vb = CHEngine::RenderFacade::CreateVertexBuffer(verts, static_cast<uint32_t>(sizeof(verts)));
    CHEngine::BufferLayout layout = {
        { CHEngine::ShaderDataType::Float2, "a_NDC" },
    };
    vb->SetLayout(layout);
    vao->AddVertexBuffer(vb);
    auto ib = CHEngine::RenderFacade::CreateIndexBuffer(indices, 6u);
    vao->SetIndexBuffer(ib);
}

} // namespace Sandbox
