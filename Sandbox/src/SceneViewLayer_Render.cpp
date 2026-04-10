#include "SceneViewLayer.h"

#include <CHEngine/Render/RenderFacade.h>
#include <Render/UniformBlocks.h>

#include <glm/gtc/type_ptr.hpp>
#include <cstring>

// ── Вспомогательная функция: Transform → float[16] matrix ─────────────────
static void TransformToMatrix(const CHEngine::Transform& tr, float out[16])
{
    float t[3] = { tr.Position.x, tr.Position.y, tr.Position.z };
    float r[3] = { tr.Rotation.x, tr.Rotation.y, tr.Rotation.z };
    float s[3] = { tr.Scale.x,    tr.Scale.y,    tr.Scale.z    };
    ImGuizmo::RecomposeMatrixFromComponents(t, r, s, out);
}

// ============================================================================
//  Grid / axes geometry
// ============================================================================

void SceneViewLayer::BuildGrid()
{
    float verts[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f,
        -1.0f,  1.0f,
    };
    uint32_t indices[] = { 0, 1, 2, 0, 2, 3 };

    m_GridVAO = CHEngine::RenderFacade::CreateVertexArray();
    auto* vao = CHEngine::RenderFacade::GetVertexArray(m_GridVAO);
    if (!vao) return;

    auto vb = CHEngine::RenderFacade::CreateVertexBuffer(verts, static_cast<uint32_t>(sizeof(verts)));
    CHEngine::BufferLayout layout = {
        { CHEngine::ShaderDataType::Float2, "a_NDC" },
    };
    vb->SetLayout(layout);
    vao->AddVertexBuffer(vb);
    auto ib = CHEngine::RenderFacade::CreateIndexBuffer(indices, 6u);
    vao->SetIndexBuffer(ib);
}

// ============================================================================
//  Rendering
// ============================================================================

void SceneViewLayer::RenderScene(CHEngine::Timestep dt)
{
    CHE_PROFILE_FUNCTION();

    // ── Bind FBO — all rendering goes into the offscreen texture ─────────
    auto* fbo = CHEngine::RenderFacade::GetFramebuffer(m_Framebuffer);
    if (fbo) fbo->Bind();
    CHEngine::RenderFacade::Clear();
    // ─────────────────────────────────────────────────────────────────────

    glm::mat4 vp    = m_Camera.GetViewProjectionMatrix(m_AspectRatio);
    glm::mat4 invVP = glm::inverse(vp);
    glm::vec3 camPos = m_Camera.GetPosition();

    // ── Заполняем CameraUBO (раз в кадр, для всех шейдеров) ──────────────
    CHEngine::UBOCamera cameraUBO;
    std::memcpy(cameraUBO.ViewProjection, glm::value_ptr(vp),    64);
    std::memcpy(cameraUBO.InvViewProj,    glm::value_ptr(invVP), 64);
    cameraUBO.CameraPos[0] = camPos.x;
    cameraUBO.CameraPos[1] = camPos.y;
    cameraUBO.CameraPos[2] = camPos.z;
    cameraUBO.CameraPos[3] = 0.0f;

    CHEngine::RenderFacade::SetSceneCamera(cameraUBO);

    // Infinite grid (full-screen quad + analytical fragment shader)
    if (m_ShowGrid)
    {
        if (m_GridShader.IsValid() && m_GridVAO.IsValid())
        {
            CHEngine::RenderFacade::SetBlend(true);
            CHEngine::RenderFacade::SetDepthWrite(false);

            CHEngine::RenderFacade::Submit(m_GridShader, m_GridVAO, glm::mat4(1.0f));

            CHEngine::RenderFacade::SetDepthWrite(true);
            CHEngine::RenderFacade::SetBlend(false);
        }
    }

    // Mesh-pass moved to ECS Presentation phase (RenderSystem).
    m_World.update(dt);

    // ── Unbind FBO — restore default framebuffer ─────────────────────────
    if (fbo) fbo->Unbind();
    // ─────────────────────────────────────────────────────────────────────
}

// ============================================================================
//  UI — Transform gizmo
// ============================================================================

void SceneViewLayer::DrawGizmo()
{
    // Гизмо недоступен в Play/Pause режиме
    if (m_EditorState != EditorState::Edit) return;

    auto handle = m_Scene.TryGetEntityHandleByUUID(m_SelectedObjectID);
    auto* entity = m_Scene.TryGetEntity(handle);
    if (!entity || !entity->HasComponent<CHEngine::TransformComponent>()) return;
    auto& selectedTransform = entity->GetComponent<CHEngine::TransformComponent>().ObjectTransform;

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();
    ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::SetRect(m_ViewportPos.x, m_ViewportPos.y, m_ViewportSize.x, m_ViewportSize.y);

    glm::mat4 view = m_Camera.GetViewMatrix();
    glm::mat4 proj = m_Camera.GetProjectionMatrix(m_AspectRatio);

    float t[3], r[3], s[3];
    float mm[16];
    TransformToMatrix(selectedTransform, mm);

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

    if (!m_GizmoWasUsing && ImGuizmo::IsUsing())
        m_TransformBeforeDrag = selectedTransform;

    ImGuizmo::Manipulate(
        glm::value_ptr(view), glm::value_ptr(proj),
        m_GizmoOperation, m_GizmoMode, mm,
        nullptr, snap);

    if (ImGuizmo::IsUsing())
    {
        ImGuizmo::DecomposeMatrixToComponents(mm, t, r, s);
        selectedTransform.Position = { t[0], t[1], t[2] };
        selectedTransform.Rotation = { r[0], r[1], r[2] };
        selectedTransform.Scale    = { s[0], s[1], s[2] };
    }

    if (m_GizmoWasUsing && !ImGuizmo::IsUsing())
        m_UndoStack.PushTransform(&m_Scene, m_SelectedObjectID, m_TransformBeforeDrag);

    m_GizmoWasUsing = ImGuizmo::IsUsing();
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
