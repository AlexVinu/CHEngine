#include "EditorViewport.h"

#include <CHEngine/Application.h>
#include <CHEngine/Render/RenderFacade.h>
#include <Render/UniformBlocks.h>

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
    }
}

void EditorViewport::RegisterEditorPasses(SceneSession* scene_session)
{
    // TODO: register editor grid pass after Phase 6 (frame graph backend implemented).
    (void)scene_session;
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
    ImGui::Begin("##viewport", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
                     | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus
                     | ImGuiWindowFlags_NoNavFocus);

    m_ViewportHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    m_ViewportPos = ImGui::GetWindowPos();

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
        ImGui::Image(reinterpret_cast<ImTextureID>(colorTexID), panelSize, uv0, uv1);
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
    // TODO: rebuild grid using new handle-based VertexArray after Phase 6.
    // For now grid rendering is disabled (RegisterEditorPasses is a no-op).
}

} // namespace Sandbox
