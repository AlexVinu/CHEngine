#include "SceneViewLayer_MousePicking.h"

#include "SceneViewLayer.h"
#include "SceneViewLayerAccess.h"
#include "EditorPopupState.h"

#include <CHEngine/Application.h>
#include <CHEngine/Camera/EditorCamera.h>
#include <CHEngine/Scene/Components.h>
#include <CHEngine/Scene/Entity.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <ImGuizmo.h>

#include <algorithm>
#include <limits>

namespace SceneViewLayerMousePicking {

namespace {

struct AABB
{
    glm::vec3 Min{ std::numeric_limits<float>::max() };
    glm::vec3 Max{ std::numeric_limits<float>::lowest() };

    void Expand(const glm::vec3& point)
    {
        Min = glm::min(Min, point);
        Max = glm::max(Max, point);
    }
};

AABB ComputeWorldAABB(const CHEngine::MeshComponent& meshComp, const glm::mat4& model)
{
    CHEngine::MeshLoader* meshLoader = CHEngine::Application::Get().Resources().GetMeshLoader();
    AABB aabb;
    if (meshComp.Mesh.IsValid())
    {
        if (const auto* rec = meshLoader->GetGpuRecord(meshComp.Mesh.Handle()))
        {
            for (const auto& v : rec->vertices)
            {
                glm::vec4 worldPos = model * glm::vec4(v.Position, 1.0f);
                aabb.Expand(glm::vec3(worldPos));
            }
        }
    }
    return aabb;
}

bool RayAABBIntersect(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                      const AABB& aabb, float& outT)
{
    float tMin = 0.0f;
    float tMax = std::numeric_limits<float>::max();

    for (int axis = 0; axis < 3; ++axis)
    {
        float invD = 1.0f / rayDir[axis];
        float t0 = (aabb.Min[axis] - rayOrigin[axis]) * invD;
        float t1 = (aabb.Max[axis] - rayOrigin[axis]) * invD;
        if (invD < 0.0f) std::swap(t0, t1);
        tMin = std::max(tMin, t0);
        tMax = std::min(tMax, t1);
        if (tMax <= tMin) return false;
    }

    outT = tMin;
    return true;
}

} // namespace

void TryPick(SceneViewLayer& layer)
{
    EditorWorldContext* ctxRef = SceneViewLayerAccess::ActiveWorldCtx(layer);
    EditorWorldContext& ctx = *ctxRef;
    if (ctx.GetSessionState() != SceneSession::State::Edit)
        return;

    if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        return;

    if (EditorPopup::AnyOpen())
        return;

    // Don't pick when the gizmo is active/under cursor — gizmo handles its own LMB drag.
    if (ImGuizmo::IsOver() || ImGuizmo::IsUsing())
        return;

    const ImVec2 vpPos  = SceneViewLayerAccess::Viewport(layer).GetViewportPos();
    const ImVec2 vpSize = SceneViewLayerAccess::Viewport(layer).GetViewportSize();

    if (vpSize.x < 1.0f || vpSize.y < 1.0f)
        return;

    auto* viewportCamera = ctx.ViewportCamera.get();
    if (!viewportCamera)
        return;

    const ImVec2 mousePos = ImGui::GetMousePos();
    const float mx = mousePos.x - vpPos.x;
    const float my = mousePos.y - vpPos.y;

    if (mx < 0.0f || my < 0.0f || mx > vpSize.x || my > vpSize.y)
        return;

    // Compute NDC
    float ndcX = 2.0f * mx / vpSize.x - 1.0f;
    float ndcY = 1.0f - 2.0f * my / vpSize.y;

    glm::mat4 viewProj = viewportCamera->GetViewProjection();
    glm::mat4 invVP    = glm::inverse(viewProj);

    glm::vec4 nearH = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 farH  = invVP * glm::vec4(ndcX, ndcY,  1.0f, 1.0f);

    glm::vec3 rayOrigin = glm::vec3(nearH) / nearH.w;
    glm::vec3 rayFar    = glm::vec3(farH) / farH.w;
    glm::vec3 rayDir    = glm::normalize(rayFar - rayOrigin);

    auto scene = ctx.EditorScene;
    if (!scene)
        return;

    CHEngine::EntityHandle closestHandle{};
    float closestT = std::numeric_limits<float>::max();

    scene->ForEach<CHEngine::MeshComponent, CHEngine::TransformComponent, CHEngine::VisibilityComponent>(
        [&](CHEngine::EntityHandle handle, const CHEngine::UUID&,
            CHEngine::MeshComponent& meshComp,
            CHEngine::TransformComponent& transformComp,
            CHEngine::VisibilityComponent& visibilityComp)
        {
            if (!visibilityComp.Visible)
                return;
            if (!meshComp.Mesh.IsValid())
                return;

            const CHEngine::Transform& t = transformComp.ObjectTransform;
            glm::mat4 model = glm::translate(glm::mat4(1.0f), t.Position)
                            * glm::mat4_cast(glm::quat(glm::radians(t.Rotation)))
                            * glm::scale(glm::mat4(1.0f), t.Scale);

            float tHit = std::numeric_limits<float>::max();
            bool hit = false;

            // Sphere impostors: the mesh is a flat billboard quad (z=0 vertices),
            // so AABB-based picking fails. Use analytic ray-sphere intersection instead.
            if (scene->GetString(meshComp.SourcePath) == ":primitive:sphere")
            {
                // Sphere center = translation column; radius = scale magnitude
                glm::vec3 center = glm::vec3(model[3]);
                // scale * base mesh radius (0.5) — matches sphere_impostor.slang
                float radius     = glm::length(glm::vec3(model[0])) * 0.5f;

                glm::vec3 oc = rayOrigin - center;
                float b = glm::dot(rayDir, oc);
                float c = glm::dot(oc, oc) - radius * radius;
                float disc = b * b - c;
                if (disc >= 0.0f)
                {
                    float tt = -b - glm::sqrt(disc);
                    if (tt > 0.0f) { tHit = tt; hit = true; }
                }
            }
            else
            {
                AABB aabb = ComputeWorldAABB(meshComp, model);
                hit = RayAABBIntersect(rayOrigin, rayDir, aabb, tHit) && tHit > 0.0f;
            }

            if (hit && tHit < closestT)
            {
                closestT = tHit;
                closestHandle = handle;
            }
        });

    ctx.SelectedEntity = closestHandle;
}

} // namespace SceneViewLayerMousePicking
