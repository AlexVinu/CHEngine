#include "SceneViewLayer_Render.h"

#include "SceneViewLayer.h"
#include "SceneViewLayerAccess.h"

#include <CHEngine/Camera/EditorCamera.h>

#include <glm/glm.hpp>

#include <imgui.h>

namespace SceneViewLayerRender {

void DrawOrbitIndicator(SceneViewLayer& layer)
{
    EditorWorldContext& ctx = SceneViewLayerAccess::Active(layer);
    if (ctx.SessionState == SceneSession::State::Play || ctx.SessionState == SceneSession::State::Pause)
        return;

    ImGuiIO& io = ImGui::GetIO();
    const float W = io.DisplaySize.x;
    const float H = io.DisplaySize.y;

    auto* viewport_camera = ctx.ViewportCamera.get();
    if (!viewport_camera)
        return;
    glm::mat4 view = viewport_camera->GetViewMatrix();
    glm::mat4 proj = viewport_camera->GetProjectionMatrix();

    glm::vec4 clip = proj * view * glm::vec4(ctx.m_EditorCameraState.OrbitTarget, 1.0f);
    if (clip.w <= 0.0f)
        return;

    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    if (glm::abs(ndc.x) > 1.0f || glm::abs(ndc.y) > 1.0f)
        return;

    float sx = (ndc.x + 1.0f) * 0.5f * W;
    float sy = (1.0f - ndc.y) * 0.5f * H;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const float r = 6.0f;
    const float gap = 2.5f;
    const ImU32 col = IM_COL32(255, 255, 255, 140);
    const ImU32 out = IM_COL32(0, 0, 0, 80);

    dl->AddLine({ sx - r - 1, sy }, { sx - gap - 1, sy }, out, 1.5f);
    dl->AddLine({ sx + gap - 1, sy }, { sx + r - 1, sy }, out, 1.5f);
    dl->AddLine({ sx, sy - r - 1 }, { sx, sy - gap - 1 }, out, 1.5f);
    dl->AddLine({ sx, sy + gap - 1 }, { sx, sy + r - 1 }, out, 1.5f);

    dl->AddLine({ sx - r, sy }, { sx - gap, sy }, col, 1.5f);
    dl->AddLine({ sx + gap, sy }, { sx + r, sy }, col, 1.5f);
    dl->AddLine({ sx, sy - r }, { sx, sy - gap }, col, 1.5f);
    dl->AddLine({ sx, sy + gap }, { sx, sy + r }, col, 1.5f);

    dl->AddCircleFilled({ sx, sy }, 1.8f, col);
}

} // namespace SceneViewLayerRender
