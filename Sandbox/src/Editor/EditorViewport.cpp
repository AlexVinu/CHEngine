#include "EditorViewport.h"

#include <CHEngine/Application.h>
#include <CHEngine/Render/RenderFacade.h>
#include <Render/UniformBlocks.h>

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
    m_MeshShader = CHEngine::RenderFacade::CreateShaderFromFile(
        CHEngine::String("Mesh"),
        CHEngine::String("shaders/mesh.slang"));
    CHEngine::RenderFacade::SetDefaultMeshShader(m_MeshShader);

    m_GridShader = CHEngine::RenderFacade::CreateShaderFromFile(
        CHEngine::String("Grid"),
        CHEngine::String("shaders/grid.slang"));

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
        std::memcpy(cameraUBO.ViewProjection, glm::value_ptr(vp),    sizeof(cameraUBO.ViewProjection));
        std::memcpy(cameraUBO.InvViewProj,    glm::value_ptr(invVP), sizeof(cameraUBO.InvViewProj));
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

// ─────────────────────────────────────────────────────────────────────────────
// Вспомогательные функции визуализации камеры
// ─────────────────────────────────────────────────────────────────────────────
namespace {

// Проецирует мировую точку в пиксели вьюпорта
// Возвращает false если точка за камерой (w < 0)
bool WorldToViewport(const glm::vec3& worldPos,
                     const glm::mat4& editorVP,
                     const ImVec2& vpPos,
                     const ImVec2& vpSize,
                     ImVec2& outScreen)
{
    glm::vec4 clip = editorVP * glm::vec4(worldPos, 1.0f);
    if (clip.w < 0.001f) return false;               // за камерой — не рисуем

    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    if (ndc.x < -1.5f || ndc.x > 1.5f ||
        ndc.y < -1.5f || ndc.y > 1.5f) return false; // сильно вне экрана

    outScreen.x = vpPos.x + (ndc.x * 0.5f + 0.5f) * vpSize.x;
    outScreen.y = vpPos.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * vpSize.y;
    return true;
}

// Линия между двумя мировыми точками через ImDrawList
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

// Строит матрицу вида из Transform (Position + Rotation Euler degrees)
glm::mat4 TransformToViewMatrix(const CHEngine::Transform& t)
{
    glm::mat4 rot = glm::eulerAngleYXZ(
        glm::radians(t.Rotation.y),
        glm::radians(t.Rotation.x),
        glm::radians(t.Rotation.z));

    glm::mat4 model = glm::translate(glm::mat4(1.0f), t.Position) * rot;
    return glm::inverse(model);
}

// Рисует фрустум камеры + иконку корпуса
void DrawCameraFrustum(ImDrawList* dl,
                       const CHEngine::Transform& transform,
                       const CHEngine::SceneCamera& cam,
                       const glm::mat4& editorVP,
                       const ImVec2& vpPos, const ImVec2& vpSize,
                       bool isSelected)
{
    const ImU32 colorFrustum  = isSelected
        ? IM_COL32(255, 220,  50, 220)   // жёлтый — выбранная
        : IM_COL32(180, 220, 255, 160);  // голубой — обычная
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

    // Строим фрустум вручную через FOV/aspect — не зависит от near/far камеры
    // Показываем near plane и визуальный far (~8 единиц) чтобы было наглядно
    float fov    = cam.GetPerspectiveVerticalFOV();           // radians
    float aspect = cam.GetProjectionType() ==
                   CHEngine::SceneCamera::ProjectionType::Perspective
                   ? 16.0f / 9.0f : 1.0f;
    float nearD  = cam.GetPerspectiveNearClip();
    float farD   = std::min(cam.GetPerspectiveFarClip(), 8.0f); // показываем до 8 ед.

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

    // Near plane (прямоугольник)
    DrawWorldLine(dl, wPts[0], wPts[1], editorVP, vpPos, vpSize, colorFrustum, lineW);
    DrawWorldLine(dl, wPts[1], wPts[2], editorVP, vpPos, vpSize, colorFrustum, lineW);
    DrawWorldLine(dl, wPts[2], wPts[3], editorVP, vpPos, vpSize, colorFrustum, lineW);
    DrawWorldLine(dl, wPts[3], wPts[0], editorVP, vpPos, vpSize, colorFrustum, lineW);

    // Far plane (прямоугольник)
    DrawWorldLine(dl, wPts[4], wPts[5], editorVP, vpPos, vpSize, colorFrustum, lineW);
    DrawWorldLine(dl, wPts[5], wPts[6], editorVP, vpPos, vpSize, colorFrustum, lineW);
    DrawWorldLine(dl, wPts[6], wPts[7], editorVP, vpPos, vpSize, colorFrustum, lineW);
    DrawWorldLine(dl, wPts[7], wPts[4], editorVP, vpPos, vpSize, colorFrustum, lineW);

    // Рёбра near→far
    for (int i = 0; i < 4; ++i)
        DrawWorldLine(dl, wPts[i], wPts[i + 4], editorVP, vpPos, vpSize, colorFrustum, lineW);

    // ── Иконка корпуса камеры ─────────────────────────────────────────────────
    auto rotVec = [&](glm::vec3 v) -> glm::vec3 {
        return glm::vec3(rot * glm::vec4(v, 0.0f));
    };

    const float bW = 0.18f, bH = 0.12f, bD = 0.24f;  // размер "корпуса"

    glm::vec3 right   = rotVec({ 1, 0, 0 });
    glm::vec3 up      = rotVec({ 0, 1, 0 });
    glm::vec3 fwd     = rotVec({ 0, 0, -1 });

    // 8 углов прямоугольного тела
    glm::vec3 box[8];
    for (int xi = -1; xi <= 1; xi += 2)
    for (int yi = -1; yi <= 1; yi += 2)
    for (int zi =  0; zi <= 1; zi += 1)
    {
        int idx = ((xi+1)/2)*4 + ((yi+1)/2)*2 + zi;
        box[idx] = camPos + right*((float)xi*bW) + up*((float)yi*bH) + fwd*(zi == 0 ? bD*0.5f : -bD*0.5f);
    }

    // Рёбра тела (12 линий куба)
    int edges[12][2] = {
        {0,2},{2,6},{6,4},{4,0},  // back face
        {1,3},{3,7},{7,5},{5,1},  // front face
        {0,1},{2,3},{4,5},{6,7}   // connecting
    };
    for (auto& e : edges)
        DrawWorldLine(dl, box[e[0]], box[e[1]], editorVP, vpPos, vpSize, colorBody, 1.0f);

    // Треугольник "видоискатель" сверху
    glm::vec3 tvBase1 = camPos + right*( bW) + up*(bH) + fwd*(bD*0.45f);
    glm::vec3 tvBase2 = camPos + right*(-bW) + up*(bH) + fwd*(bD*0.45f);
    glm::vec3 tvTip   = camPos                           + up*(bH*2.2f) + fwd*(bD*0.2f);
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
