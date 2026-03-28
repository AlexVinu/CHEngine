#include "SceneViewLayer.h"

#include <Render/UniformBlocks.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <cstring>

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
    CHE_PROFILE_FUNCTION();
    auto& res = m_Resources;
    auto* api = res.Get(m_RenderApi);
    if (!api) return;

    // ── Bind FBO — all rendering goes into the offscreen texture ─────────
    auto* fbo = res.Get(m_Framebuffer);
    if (fbo) fbo->Bind();
    api->Clear();
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

    // Infinite grid (full-screen quad + analytical fragment shader)
    if (m_ShowGrid)
    {
        auto* gs  = res.Get(m_GridShader);
        auto* gva = res.Get(m_GridVAO);
        if (gs && gva)
        {
            api->SetBlend(true);
            api->SetDepthWrite(false);

            gs->Bind();
            gs->SetUniformBlock(CHEngine::EUniformBlock::Camera,
                                &cameraUBO, sizeof(cameraUBO));
            api->DrawIndexed(gva);

            api->SetDepthWrite(true);
            api->SetBlend(false);
        }
    }

    // ── Meshes ────────────────────────────────────────────────────────────
    auto* shader = res.Get(m_MeshShader);
    if (!shader) return;

    shader->Bind();
    shader->SetUniformBlock(CHEngine::EUniformBlock::Camera,
                            &cameraUBO, sizeof(cameraUBO));

    // ── Заполняем LightingUBO ─────────────────────────────────────────────
    CHEngine::UBOLighting lightingUBO;
    lightingUBO.AmbientColor[0] = DefaultLight::AmbR;
    lightingUBO.AmbientColor[1] = DefaultLight::AmbG;
    lightingUBO.AmbientColor[2] = DefaultLight::AmbB;
    lightingUBO.AmbientColor[3] = 0.0f;

    int lightCount = 0;

    m_Scene.ForEach<CHEngine::LightComponent>(
        [&](CHEngine::EntityHandle handle, CHEngine::TagComponentIDType,
            CHEngine::LightComponent& lightComp) {
        const auto* visible = m_Scene.TryGetComponent<CHEngine::VisibilityComponent>(handle);
        if (!visible || !visible->Visible) return;
        auto* transformComp = m_Scene.TryGetComponent<CHEngine::TransformComponent>(handle);
        if (!transformComp) return;
        auto& transform = transformComp->ObjectTransform;
        auto& light = lightComp.LightData;
        if (lightCount >= CHEngine::MaxUBOLights) return;
        if (light.Type == CHEngine::LightType::None) return;
        auto& L     = lightingUBO.Lights[lightCount];

        L.Type = static_cast<int32_t>(light.Type);

        L.Position[0] = transform.Position.x;
        L.Position[1] = transform.Position.y;
        L.Position[2] = transform.Position.z;
        L.Position[3] = 0.0f;

        glm::vec3 dir = ForwardFromRotation(transform.Rotation);
        L.Direction[0] = dir.x;
        L.Direction[1] = dir.y;
        L.Direction[2] = dir.z;
        L.Direction[3] = 0.0f;

        L.ColorIntensity[0] = light.Color.r;
        L.ColorIntensity[1] = light.Color.g;
        L.ColorIntensity[2] = light.Color.b;
        L.ColorIntensity[3] = light.Intensity;

        L.Range     = light.Range;
        L.InnerCone = std::cos(glm::radians(light.InnerCone));
        L.OuterCone = std::cos(glm::radians(light.OuterCone));

        ++lightCount;
    });

    // Если нет источников — добавляем дефолтный directional
    if (lightCount == 0)
    {
        auto& L = lightingUBO.Lights[0];
        L.Type = 0;
        L.Direction[0] = -0.3f;
        L.Direction[1] = -1.0f;
        L.Direction[2] = -0.5f;
        L.Direction[3] = 0.0f;
        L.ColorIntensity[0] = 1.0f;
        L.ColorIntensity[1] = 1.0f;
        L.ColorIntensity[2] = 0.95f;
        L.ColorIntensity[3] = 1.0f;
        lightCount = 1;
    }

    lightingUBO.NumLights = lightCount;

    shader->SetUniformBlock(CHEngine::EUniformBlock::Lighting,
                            &lightingUBO, sizeof(lightingUBO));

    // ── Рисуем объекты ────────────────────────────────────────────────────
    m_Scene.ForEach<CHEngine::MeshComponent>(
        [&](CHEngine::EntityHandle handle, CHEngine::TagComponentIDType objectID,
            CHEngine::MeshComponent& meshComp) {
        const auto* visible = m_Scene.TryGetComponent<CHEngine::VisibilityComponent>(handle);
        if (!visible || !visible->Visible) return;
        auto* transformComp = m_Scene.TryGetComponent<CHEngine::TransformComponent>(handle);
        auto* colorComp = m_Scene.TryGetComponent<CHEngine::ColorComponent>(handle);
        if (!transformComp || !colorComp) return;
        auto& transform = transformComp->ObjectTransform;
        auto& color = colorComp->Color;

        float raw[16];
        TransformToMatrix(transform, raw);
        glm::mat4 model     = glm::make_mat4(raw);
        glm::mat4 normalMat = glm::transpose(glm::inverse(model));

        // Заполняем ObjectUBO
        CHEngine::UBOObject objectUBO;
        std::memcpy(objectUBO.Transform,    glm::value_ptr(model),     64);
        std::memcpy(objectUBO.NormalMatrix, glm::value_ptr(normalMat), 64);
        objectUBO.Color[0] = color.r;
        objectUBO.Color[1] = color.g;
        objectUBO.Color[2] = color.b;
        objectUBO.Color[3] = color.a;
        objectUBO.Selected = (objectID == m_SelectedObjectID) ? 1.0f : 0.0f;

        for (auto& mesh : meshComp.Meshes)
        {
            const auto& mat = mesh.Mat;

            // Diffuse текстура (слот 0)
            auto* diffTex = mat.DiffuseMap.IsValid() ? res.Get(mat.DiffuseMap) : nullptr;
            objectUBO.UseTexture = diffTex ? 1 : 0;
            if (diffTex) {
                diffTex->Bind(0);
                shader->SetInt(CHEngine::String("u_DiffuseTexture"), 0);
            }

            // Specular текстура (слот 1)
            auto* specTex = mat.SpecularMap.IsValid() ? res.Get(mat.SpecularMap) : nullptr;
            objectUBO.UseSpecularMap = specTex ? 1 : 0;
            if (specTex) {
                specTex->Bind(1);
                shader->SetInt(CHEngine::String("u_SpecularMap"), 1);
            }

            objectUBO.Shininess = mat.Shininess;

            // Отправляем ObjectUBO
            shader->SetUniformBlock(CHEngine::EUniformBlock::Object,
                                    &objectUBO, sizeof(objectUBO));

            auto* vao = res.Get(mesh.GetVertexArray());
            if (vao) api->DrawIndexed(vao);

            if (diffTex) diffTex->Unbind();
            if (specTex) specTex->Unbind();
        }
    });

    // ── Unbind FBO — restore default framebuffer ─────────────────────────
    if (fbo) fbo->Unbind();
    // ─────────────────────────────────────────────────────────────────────
}

// ============================================================================
//  UI — Transform gizmo
// ============================================================================

void SceneViewLayer::DrawGizmo()
{
    auto handle = m_Scene.TryGetEntityHandleByID(m_SelectedObjectID);
    auto* transformComp = m_Scene.TryGetComponent<CHEngine::TransformComponent>(handle);
    if (!transformComp) return;
    auto& selectedTransform = transformComp->ObjectTransform;

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
