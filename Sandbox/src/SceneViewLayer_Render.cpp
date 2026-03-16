#include "SceneViewLayer.h"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <cstdio>

// ── Максимум источников, синхронизировано с mesh.frag ──────────────────────
static constexpr int MaxLights = 8;

// ── Дефолтный ambient (если нет источников — чтоб сцена не была чёрной) ───
namespace DefaultLight {
    constexpr float AmbR = 0.15f, AmbG = 0.15f, AmbB = 0.2f;
}

// ── Вспомогательная функция: Transform → float[16] matrix ─────────────────
static void TransformToMatrix(const CHEngine::Transform& tr, float out[16])
{
    float t[3] = { tr.Position.x, tr.Position.y, tr.Position.z };
    float r[3] = { tr.Rotation.x, tr.Rotation.y, tr.Rotation.z };
    float s[3] = { tr.Scale.x,    tr.Scale.y,    tr.Scale.z    };
    ImGuizmo::RecomposeMatrixFromComponents(t, r, s, out);
}

// ── Направление "вперёд" из эйлеровых углов (градусы) ─────────────────────
static glm::vec3 ForwardFromRotation(const glm::vec3& eulerDeg)
{
    float pitch = glm::radians(eulerDeg.x);
    float yaw   = glm::radians(eulerDeg.y);
    return glm::normalize(glm::vec3(
        std::cos(pitch) * std::sin(yaw),
       -std::sin(pitch),
        std::cos(pitch) * std::cos(yaw)
    ));
}

// ============================================================================
//  Grid / axes geometry
// ============================================================================

void SceneViewLayer::BuildGrid()
{
    auto& res = m_Resources;

    float verts[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f,
        -1.0f,  1.0f,
    };
    uint32_t indices[] = { 0, 1, 2, 0, 2, 3 };

    m_GridVAO = res.CreateVertexArray();
    auto* vao = res.Get(m_GridVAO);
    if (!vao) return;

    auto vb = res.CreateVertexBuffer(verts, static_cast<uint32_t>(sizeof(verts)));
    CHEngine::BufferLayout layout = {
        { CHEngine::ShaderDataType::Float2, "a_NDC" },
    };
    vb->SetLayout(layout);
    vao->AddVertexBuffer(vb);
    auto ib = res.CreateIndexBuffer(indices, 6u);
    vao->SetIndexBuffer(ib);
}

// ============================================================================
//  Rendering
// ============================================================================

void SceneViewLayer::RenderScene()
{
    auto& res = m_Resources;
    auto* api = res.Get(m_RenderApi);
    if (!api) return;

    // ── Bind FBO — all rendering goes into the offscreen texture ─────────
    auto* fbo = res.Get(m_Framebuffer);
    if (fbo) fbo->Bind();
    api->Clear();
    // ─────────────────────────────────────────────────────────────────────

    glm::mat4 vp = m_Camera.GetViewProjectionMatrix(m_AspectRatio);

    // Infinite grid (full-screen quad + analytical fragment shader)
    if (m_ShowGrid)
    {
        auto* gs  = res.Get(m_GridShader);
        auto* gva = res.Get(m_GridVAO);
        if (gs && gva)
        {
            glm::mat4 invVP = glm::inverse(vp);
            glm::vec3 camPos = m_Camera.GetPosition();

            api->SetBlend(true);
            api->SetDepthWrite(false);

            gs->Bind();
            gs->SetMat4  (CHEngine::String("u_InvViewProj"), glm::value_ptr(invVP));
            gs->SetFloat3(CHEngine::String("u_CameraPos"),
                          camPos.x, camPos.y, camPos.z);
            api->DrawIndexed(gva);

            api->SetDepthWrite(true);
            api->SetBlend(false);
        }
    }

    // ── Meshes ────────────────────────────────────────────────────────────
    auto* shader = res.Get(m_MeshShader);
    if (!shader) return;

    shader->Bind();
    shader->SetMat4(CHEngine::String("u_ViewProjection"), glm::value_ptr(vp));

    // Позиция камеры (для specular)
    glm::vec3 camPos = m_Camera.GetPosition();
    shader->SetFloat3(CHEngine::String("u_CameraPos"), camPos.x, camPos.y, camPos.z);

    // Ambient
    shader->SetFloat3(CHEngine::String("u_AmbientColor"),
                      DefaultLight::AmbR, DefaultLight::AmbG, DefaultLight::AmbB);

    // ── Собираем источники света из сцены ─────────────────────────────────
    int lightCount = 0;
    char nameBuf[64];

    for (auto& obj : m_Scene.GetObjects())
    {
        if (lightCount >= MaxLights) break;
        if (!obj->Visible) continue;
        if (obj->LightData.Type == CHEngine::LightType::None) continue;

        int i = lightCount++;
        const auto& light = obj->LightData;
        const auto& tr    = obj->ObjectTransform;

        snprintf(nameBuf, sizeof(nameBuf), "u_LightType[%d]", i);
        shader->SetInt(CHEngine::String(nameBuf), static_cast<int>(light.Type));

        snprintf(nameBuf, sizeof(nameBuf), "u_LightPosition[%d]", i);
        shader->SetFloat3(CHEngine::String(nameBuf), tr.Position.x, tr.Position.y, tr.Position.z);

        glm::vec3 dir = ForwardFromRotation(tr.Rotation);
        snprintf(nameBuf, sizeof(nameBuf), "u_LightDirection[%d]", i);
        shader->SetFloat3(CHEngine::String(nameBuf), dir.x, dir.y, dir.z);

        snprintf(nameBuf, sizeof(nameBuf), "u_LightColor[%d]", i);
        shader->SetFloat3(CHEngine::String(nameBuf), light.Color.r, light.Color.g, light.Color.b);

        snprintf(nameBuf, sizeof(nameBuf), "u_LightIntensity[%d]", i);
        shader->SetFloat(CHEngine::String(nameBuf), light.Intensity);

        snprintf(nameBuf, sizeof(nameBuf), "u_LightRange[%d]", i);
        shader->SetFloat(CHEngine::String(nameBuf), light.Range);

        snprintf(nameBuf, sizeof(nameBuf), "u_LightInnerCone[%d]", i);
        shader->SetFloat(CHEngine::String(nameBuf), cosf(glm::radians(light.InnerCone)));

        snprintf(nameBuf, sizeof(nameBuf), "u_LightOuterCone[%d]", i);
        shader->SetFloat(CHEngine::String(nameBuf), cosf(glm::radians(light.OuterCone)));
    }

    // Если нет источников — добавляем дефолтный directional, чтобы сцена не была чёрной
    if (lightCount == 0)
    {
        shader->SetInt   (CHEngine::String("u_LightType[0]"),      0);
        shader->SetFloat3(CHEngine::String("u_LightDirection[0]"), -0.3f, -1.0f, -0.5f);
        shader->SetFloat3(CHEngine::String("u_LightColor[0]"),     1.0f, 1.0f, 0.95f);
        shader->SetFloat (CHEngine::String("u_LightIntensity[0]"), 1.0f);
        lightCount = 1;
    }

    shader->SetInt(CHEngine::String("u_NumLights"), lightCount);

    // ── Рисуем объекты ────────────────────────────────────────────────────
    for (auto& obj : m_Scene.GetObjects())
    {
        if (!obj->Visible) continue;

        float raw[16];
        TransformToMatrix(obj->ObjectTransform, raw);
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
            auto* tex = mesh.DiffuseTexture.IsValid()
                      ? res.Get(mesh.DiffuseTexture) : nullptr;

            shader->SetInt(CHEngine::String("u_UseTexture"), tex ? 1 : 0);
            if (tex)
            {
                tex->Bind(0);
                shader->SetInt(CHEngine::String("u_DiffuseTexture"), 0);
            }

            auto* vao = res.Get(mesh.GetVertexArray());
            if (vao) api->DrawIndexed(vao);

            if (tex) tex->Unbind();
        }
    }

    // ── Unbind FBO — restore default framebuffer ─────────────────────────
    if (fbo) fbo->Unbind();
    // ─────────────────────────────────────────────────────────────────────
}

// ============================================================================
//  UI — Transform gizmo
// ============================================================================

void SceneViewLayer::DrawGizmo()
{
    CHEngine::SceneObject* selected = m_Scene.FindByID(m_SelectedObjectID);
    if (!selected) return;

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();
    ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::SetRect(m_ViewportPos.x, m_ViewportPos.y, m_ViewportSize.x, m_ViewportSize.y);

    glm::mat4 view = m_Camera.GetViewMatrix();
    glm::mat4 proj = m_Camera.GetProjectionMatrix(m_AspectRatio);

    float t[3], r[3], s[3];
    float mm[16];
    TransformToMatrix(selected->ObjectTransform, mm);

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

    if (m_GizmoWasUsing && !ImGuizmo::IsUsing())
        m_UndoStack.PushTransform(&m_Scene, selected->ID, m_TransformBeforeDrag);

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
