#include "PropertiesPanel.h"

#include "EditorViewport.h"
#include "SceneSession.h"
#include "SetTransformCommand.h"

#include <CHEngine/Utils/FileDialog.h>
#include <CHEngine/Mesh/Material.h>
#include <CHEngine/Render/RenderFacade.h>
#include <CHEngine/Scene/Components.h>
#include <CHEngine/Scene/Light.h>
#include <CHEngine/Scene/SceneCamera.h>

#include "UIThemeActive.h"

#include <boost/uuid/uuid_io.hpp>

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

namespace Sandbox {

namespace {

int LightTypeToComboIndex(CHEngine::LightType t)
{
    switch (t)
    {
    case CHEngine::LightType::None:
        return 0;
    case CHEngine::LightType::Directional:
        return 1;
    case CHEngine::LightType::Point:
        return 2;
    case CHEngine::LightType::Spot:
        return 3;
    default:
        return 0;
    }
}

CHEngine::LightType ComboIndexToLightType(int idx)
{
    static constexpr CHEngine::LightType kMap[] = {
        CHEngine::LightType::None,
        CHEngine::LightType::Directional,
        CHEngine::LightType::Point,
        CHEngine::LightType::Spot,
    };
    if (idx < 0 || idx >= 4)
        return CHEngine::LightType::None;
    return kMap[idx];
}

template<typename T, typename Fn>
void DrawComponent(const char* displayName,
    bool allowRemove,
    bool propsReadOnly,
    [[maybe_unused]] SceneSession& activeSession,
    [[maybe_unused]] CHEngine::EntityHandle selectedHandle,
    CHEngine::Entity* entity,
    const char* idSuffix,
    Fn&& fn)
{
    if (!entity || !entity->HasComponent<T>())
        return;

    ImGui::PushID(idSuffix);
    bool removeRequested = false;

    const ImGuiTreeNodeFlags nodeFlags = static_cast<ImGuiTreeNodeFlags>(ImGuiTreeNodeFlags_DefaultOpen
        | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap
        | ImGuiTreeNodeFlags_FramePadding);

    char nodeId[192];
    std::snprintf(nodeId, sizeof(nodeId), "%s##hdr_%s", displayName, idSuffix);
    const bool open = ImGui::TreeNodeEx(nodeId, nodeFlags, "%s", displayName);

    if (ImGui::BeginPopupContextItem())
    {
        if (allowRemove && !propsReadOnly && ImGui::MenuItem("Remove Component"))
            removeRequested = true;
        ImGui::EndPopup();
    }

    {
        const float gear = ImGui::GetFrameHeight();
        const float pad = ImGui::GetStyle().FramePadding.x;
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - gear - pad);
        if (ImGui::SmallButton("...##gear"))
            ImGui::OpenPopup("gearctx");
    }
    if (ImGui::BeginPopup("gearctx"))
    {
        ImGui::TextDisabled("No settings");
        ImGui::EndPopup();
    }

    if (open)
    {
        fn(entity->GetComponent<T>());
        ImGui::TreePop();
    }

    if (removeRequested)
    {
        entity->RemoveComponent<T>();
    }

    ImGui::PopID();
}

template<typename T, typename... Args>
void DisplayAddComponentEntry(const char* menuLabel, CHEngine::Entity* entity, [[maybe_unused]] SceneSession& session, Args&&... ctorArgs)
{
    if (!entity || entity->HasComponent<T>())
        return;
    if (ImGui::MenuItem(menuLabel))
    {
        entity->AddComponent<T>(std::forward<Args>(ctorArgs)...);
        ImGui::CloseCurrentPopup();
    }
}

} // namespace

void PropertiesPanel::Draw(EditorUiHost& host, ImVec2 pos, ImVec2 size, bool reset_layout)
{
    UIActive::BeginPanel("Properties", pos, size, 0, reset_layout);
    SceneSession& activeSession = host.GetActiveSceneSession();

    auto scene_ptr = activeSession.EditorScene;
    if (!scene_ptr)
    {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.686f, 0.686f, 0.706f, 1.0f));
        ImGui::TextUnformatted("No scene loaded.");
        ImGui::PopStyleColor();
        UIActive::EndPanel();
        return;
    }

    const CHEngine::EntityHandle selectedHandle = activeSession.SelectedEntity;
    if (!scene_ptr->IsEntityHandleValid(selectedHandle))
    {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.686f, 0.686f, 0.706f, 1.0f));
        ImGui::TextUnformatted("No object selected");
        ImGui::PopStyleColor();
        UIActive::EndPanel();
        return;
    }

    auto* selectedEntity = scene_ptr->TryGetEntity(selectedHandle);
    if (!selectedEntity || !selectedEntity->HasComponent<CHEngine::TransformComponent>())
    {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.686f, 0.686f, 0.706f, 1.0f));
        ImGui::TextUnformatted("Selected entity has no Transform (invalid or incomplete entity).");
        ImGui::PopStyleColor();
        UIActive::EndPanel();
        return;
    }

    const bool propsReadOnly = (activeSession.SessionState != SceneSession::State::Edit);
    if (propsReadOnly)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.7f, 0.1f, 1.0f));
        ImGui::TextUnformatted(activeSession.SessionState == SceneSession::State::Play ? "\xe2\x96\xb6 Play mode — read only"
                : "\xe2\x8f\xb8 Paused — read only");
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::BeginDisabled(true);
    }

    if (!selectedEntity->HasComponent<CHEngine::TagComponent>())
    {
        const std::string uuidLabel = boost::uuids::to_string(scene_ptr->GetUUID(selectedHandle));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
        ImGui::TextUnformatted("(no tag)");
        ImGui::PopStyleColor();
        char uuidBuf[256];
        std::strncpy(uuidBuf, uuidLabel.c_str(), sizeof(uuidBuf));
        uuidBuf[sizeof(uuidBuf) - 1] = '\0';
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##uuidLabel", uuidBuf, sizeof(uuidBuf), ImGuiInputTextFlags_ReadOnly);
    }

    if (!propsReadOnly)
    {
        if (ImGui::Button("Add Component", ImVec2(-1.0f, 0.0f)))
            ImGui::OpenPopup("AddComponentPopup");
        if (ImGui::BeginPopup("AddComponentPopup"))
        {
            DisplayAddComponentEntry<CHEngine::TagComponent>(
                "Tag", selectedEntity, activeSession, CHEngine::TagComponent{ "Object" });
            DisplayAddComponentEntry<CHEngine::MeshComponent>("Mesh", selectedEntity, activeSession);
            DisplayAddComponentEntry<CHEngine::ColorComponent>("Color", selectedEntity, activeSession);
            DisplayAddComponentEntry<CHEngine::VisibilityComponent>("Visibility", selectedEntity, activeSession);
            DisplayAddComponentEntry<CHEngine::LightComponent>("Light",
                selectedEntity,
                activeSession,
                CHEngine::LightComponent{ CHEngine::Light{ CHEngine::LightType::Directional } });
            DisplayAddComponentEntry<CHEngine::CameraComponent>("Camera", selectedEntity, activeSession);
            DisplayAddComponentEntry<CHEngine::RigidBody3DComponent>("RigidBody 3D", selectedEntity, activeSession);
            DisplayAddComponentEntry<CHEngine::LifetimeComponent>("Lifetime", selectedEntity, activeSession);
            ImGui::EndPopup();
        }
        ImGui::Spacing();
    }

    const float labelW = 64.0f;

    DrawComponent<CHEngine::TagComponent>("Tag",
        false,
        propsReadOnly,
        activeSession,
        selectedHandle,
        selectedEntity,
        "Tag",
        [&](CHEngine::TagComponent& tag) {
            char nameBuf[256];
            std::strncpy(nameBuf, tag.Name.c_str(), sizeof(nameBuf));
            nameBuf[sizeof(nameBuf) - 1] = '\0';
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::InputText("##nameTagComp", nameBuf, sizeof(nameBuf)))
                tag.Name = nameBuf;
        });

    DrawComponent<CHEngine::TransformComponent>("Transform",
        false,
        propsReadOnly,
        activeSession,
        selectedHandle,
        selectedEntity,
        "Transform",
        [&](CHEngine::TransformComponent& tc) {
            CHEngine::Transform& transform = tc.ObjectTransform;
            auto row = [&](const char* label, const char* id, glm::vec3& vec, float speed, float mn = -FLT_MAX, float mx = FLT_MAX) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
                ImGui::TextUnformatted(label);
                ImGui::PopStyleColor();
                ImGui::SameLine(labelW);
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::DragFloat3(id, glm::value_ptr(vec), speed, mn, mx);
                if (ImGui::IsItemActivated())
                    host.GetTransformBeforeDrag() = transform;
                if (ImGui::IsItemDeactivatedAfterEdit())
                {
                    CHEngine::Transform after = transform;
                    host.GetCommandStack().Push(CHEngine::MakeScope<SetTransformCommand>(
                        scene_ptr, selectedHandle, host.GetTransformBeforeDrag(), after));
                }
            };
            row("Position", "##posTr", transform.Position, 0.05f);
            row("Rotation", "##rotTr", transform.Rotation, 0.5f);
            row("Scale", "##sclTr", transform.Scale, 0.01f, 0.001f, 1000.0f);

            ImGui::Spacing();
            if (ImGui::Button("Reset Transform##resetTr", ImVec2(-1.0f, 0.0f)))
            {
                CHEngine::Transform before = transform;
                host.GetCommandStack().Push(CHEngine::MakeScope<SetTransformCommand>(
                    scene_ptr, selectedHandle, before, CHEngine::Transform{}));
            }
        });

    DrawComponent<CHEngine::ColorComponent>("Color",
        true,
        propsReadOnly,
        activeSession,
        selectedHandle,
        selectedEntity,
        "Color",
        [&](CHEngine::ColorComponent& colorComp) {
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::ColorEdit4("##colorComp", glm::value_ptr(colorComp.Color));
            ImGui::Spacing();
        });

    DrawComponent<CHEngine::VisibilityComponent>("Visibility",
        true,
        propsReadOnly,
        activeSession,
        selectedHandle,
        selectedEntity,
        "Visibility",
        [&](CHEngine::VisibilityComponent& visibility) {
            bool before = visibility.Visible;
            if (UIActive::Toggle("Visible##visComp", &visibility.Visible) && visibility.Visible != before)
            {
                host.GetCommandStack().Push(CHEngine::MakeScope<SetVisibilityCommand>(
                    scene_ptr, scene_ptr->GetUUID(selectedHandle), before, visibility.Visible));
            }
            ImGui::Spacing();
        });

    DrawComponent<CHEngine::MeshComponent>("Mesh",
        true,
        propsReadOnly,
        activeSession,
        selectedHandle,
        selectedEntity,
        "Mesh",
        [&](CHEngine::MeshComponent& meshComp) {
            {
                char pathBuf[1024];
                std::strncpy(pathBuf, meshComp.SourcePath.c_str(), sizeof(pathBuf));
                pathBuf[sizeof(pathBuf) - 1] = '\0';
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
                ImGui::TextUnformatted("Source path");
                ImGui::PopStyleColor();
                ImGui::SameLine(labelW);
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::InputText("##meshSourcePathMesh", pathBuf, sizeof(pathBuf)))
                    meshComp.SourcePath = pathBuf;
                ImGui::Spacing();
            }

            for (size_t mi = 0; mi < meshComp.Meshes.size(); ++mi)
            {
                ImGui::PushID(static_cast<int>(mi));
                if (meshComp.Meshes.size() > 1)
                {
                    ImGui::Spacing();
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
                    ImGui::Text("Mesh %zu", mi);
                    ImGui::PopStyleColor();
                }

                auto& subMesh = meshComp.Meshes[mi];
                if (!subMesh.Mat)
                    subMesh.Mat = CHEngine::MaterialInstance::FromBase(
                        std::make_shared<CHEngine::Material>(host.GetEditorViewport().GetMeshShader()));
                auto mat_ref = subMesh.Mat;

                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
                ImGui::TextUnformatted("Textures");
                ImGui::PopStyleColor();

                {
                    char diffBuf[512];
                    std::string effDiff = mat_ref->EffectiveDiffuseMapPath();
                    const char* dSrc = effDiff.empty() ? "(none)" : effDiff.c_str();
                    std::strncpy(diffBuf, dSrc, sizeof(diffBuf));
                    diffBuf[sizeof(diffBuf) - 1] = '\0';

                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
                    ImGui::TextUnformatted("Diffuse");
                    ImGui::PopStyleColor();
                    ImGui::SameLine(labelW);
                    float fieldW = ImGui::GetContentRegionAvail().x - 52.0f;
                    ImGui::SetNextItemWidth(fieldW);
                    ImGui::InputText("##diffPathMesh", diffBuf, sizeof(diffBuf), ImGuiInputTextFlags_ReadOnly);
                    ImGui::SameLine(0, 4);
                    if (ImGui::Button("...##difMesh", ImVec2(24, 0)))
                    {
                        std::string path = CHEngine::FileDialog::OpenImageFile();
                        if (!path.empty())
                        {
                            const size_t submeshIndex = mi;
                            host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
                                [&host, submeshIndex, path] {
                                    host.ApplyDiffuseTextureToSelectedSubmesh(submeshIndex, path);
                                },
                                [] {}, false));
                        }
                    }
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Выбрать текстуру");
                    ImGui::SameLine(0, 4);
                    if (ImGui::Button("X##difMesh", ImVec2(20, 0)))
                    {
                        const size_t submeshIndex = mi;
                        host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
                            [&host, submeshIndex] { host.ClearDiffuseTextureOnSelectedSubmesh(submeshIndex); },
                            [] {}, false));
                    }
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Убрать текстуру");
                }

                {
                    char specBuf[512];
                    std::string effSpec = mat_ref->EffectiveSpecularMapPath();
                    const char* sSrc = effSpec.empty() ? "(none)" : effSpec.c_str();
                    std::strncpy(specBuf, sSrc, sizeof(specBuf));
                    specBuf[sizeof(specBuf) - 1] = '\0';

                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
                    ImGui::TextUnformatted("Specular");
                    ImGui::PopStyleColor();
                    ImGui::SameLine(labelW);
                    float fieldW = ImGui::GetContentRegionAvail().x - 52.0f;
                    ImGui::SetNextItemWidth(fieldW);
                    ImGui::InputText("##specPathMesh", specBuf, sizeof(specBuf), ImGuiInputTextFlags_ReadOnly);
                    ImGui::SameLine(0, 4);
                    if (ImGui::Button("...##specMesh", ImVec2(24, 0)))
                    {
                        std::string path = CHEngine::FileDialog::OpenImageFile();
                        if (!path.empty())
                        {
                            const size_t submeshIndex = mi;
                            host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
                                [&host, submeshIndex, path] {
                                    host.ApplySpecularTextureToSelectedSubmesh(submeshIndex, path);
                                },
                                [] {}, false));
                        }
                    }
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Выбрать specular карту");
                    ImGui::SameLine(0, 4);
                    if (ImGui::Button("X##specMesh", ImVec2(20, 0)))
                    {
                        const size_t submeshIndex = mi;
                        host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
                            [&host, submeshIndex] { host.ClearSpecularTextureOnSelectedSubmesh(submeshIndex); },
                            [] {}, false));
                    }
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Убрать specular карту");
                }

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
                ImGui::TextUnformatted("Shininess");
                ImGui::PopStyleColor();
                ImGui::SameLine(labelW);
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::DragFloat("##shinMesh", &mat_ref->Shininess, 0.5f, 1.0f, 256.0f, "%.1f");

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
                ImGui::TextUnformatted("Specular scale");
                ImGui::PopStyleColor();
                ImGui::SameLine(labelW);
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::DragFloat("##specScaleMesh", &mat_ref->SpecularScale, 0.02f, 0.0f, 4.0f, "%.2f");

                ImGui::PopID();
            }
        });

    DrawComponent<CHEngine::LightComponent>("Light",
        true,
        propsReadOnly,
        activeSession,
        selectedHandle,
        selectedEntity,
        "Light",
        [&](CHEngine::LightComponent& lightComp) {
            CHEngine::Light& lightData = lightComp.LightData;

            const char* lightTypes[] = { "None", "Directional", "Point", "Spot" };
            int ltIdx = LightTypeToComboIndex(lightData.Type);
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::Combo("##lightTypeLt", &ltIdx, lightTypes, 4))
                lightData.Type = ComboIndexToLightType(ltIdx);

            if (lightData.Type != CHEngine::LightType::None)
            {
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
                ImGui::TextUnformatted("Color");
                ImGui::PopStyleColor();
                ImGui::SameLine(labelW);
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::ColorEdit3("##lightColorLt", glm::value_ptr(lightData.Color));

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
                ImGui::TextUnformatted("Intensity");
                ImGui::PopStyleColor();
                ImGui::SameLine(labelW);
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::DragFloat("##lightIntensityLt", &lightData.Intensity, 0.05f, 0.0f, 100.0f, "%.2f");

                if (lightData.Type != CHEngine::LightType::Directional)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
                    ImGui::TextUnformatted("Range");
                    ImGui::PopStyleColor();
                    ImGui::SameLine(labelW);
                    ImGui::SetNextItemWidth(-1.0f);
                    ImGui::DragFloat("##lightRangeLt", &lightData.Range, 0.1f, 0.1f, 1000.0f, "%.1f");
                }

                if (lightData.Type == CHEngine::LightType::Spot)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
                    ImGui::TextUnformatted("Inner");
                    ImGui::PopStyleColor();
                    ImGui::SameLine(labelW);
                    ImGui::SetNextItemWidth(-1.0f);
                    ImGui::DragFloat("##lightInnerLt", &lightData.InnerCone, 0.5f, 0.0f, 89.0f, "%.1f\xc2\xb0");

                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
                    ImGui::TextUnformatted("Outer");
                    ImGui::PopStyleColor();
                    ImGui::SameLine(labelW);
                    ImGui::SetNextItemWidth(-1.0f);
                    ImGui::DragFloat("##lightOuterLt", &lightData.OuterCone, 0.5f, 0.0f, 89.0f, "%.1f\xc2\xb0");
                }

                ImGui::Spacing();
            }
        });

    DrawComponent<CHEngine::CameraComponent>("Camera",
        true,
        propsReadOnly,
        activeSession,
        selectedHandle,
        selectedEntity,
        "Camera",
        [&](CHEngine::CameraComponent& camComp) {
            CHEngine::SceneCamera& cam = camComp.Camera;

            const uint32_t vw = std::max(1u, static_cast<uint32_t>(activeSession.ViewportSize.x));
            const uint32_t vh = std::max(1u, static_cast<uint32_t>(activeSession.ViewportSize.y));
            auto syncViewport = [&] { cam.SetViewportSize(vw, vh); };

            if (ImGui::Checkbox("Primary##camPrim", &camComp.Primary))
                syncViewport();
            ImGui::Checkbox("Fixed aspect ratio##camFixAsp", &camComp.FixedAspectRatio);

            const char* projNames[] = { "Perspective", "Orthographic" };
            int projIdx = cam.GetProjectionType() == CHEngine::SceneCamera::ProjectionType::Perspective ? 0 : 1;
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::Combo("##camProjCam", &projIdx, projNames, 2))
            {
                cam.SetProjectionType(projIdx == 0 ? CHEngine::SceneCamera::ProjectionType::Perspective
                                                   : CHEngine::SceneCamera::ProjectionType::Orthographic);
                syncViewport();
            }

            if (cam.GetProjectionType() == CHEngine::SceneCamera::ProjectionType::Perspective)
            {
                float fovDeg = glm::degrees(cam.GetPerspectiveVerticalFOV());
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
                ImGui::TextUnformatted("FOV (deg)");
                ImGui::PopStyleColor();
                ImGui::SameLine(labelW);
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::DragFloat("##camFovCam", &fovDeg, 0.25f, 1.0f, 120.0f, "%.1f"))
                {
                    cam.SetPerspectiveVerticalFOV(glm::radians(fovDeg));
                    syncViewport();
                }

                float pn = cam.GetPerspectiveNearClip();
                float pf = cam.GetPerspectiveFarClip();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
                ImGui::TextUnformatted("Near");
                ImGui::PopStyleColor();
                ImGui::SameLine(labelW);
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::DragFloat("##camPNearCam", &pn, 0.01f, 0.001f, pf * 0.999f, "%.3f"))
                {
                    cam.SetPerspectiveNearClip(pn);
                    syncViewport();
                }
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
                ImGui::TextUnformatted("Far");
                ImGui::PopStyleColor();
                ImGui::SameLine(labelW);
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::DragFloat("##camPFarCam", &pf, 1.0f, pn * 1.001f, 100000.0f, "%.1f"))
                {
                    cam.SetPerspectiveFarClip(pf);
                    syncViewport();
                }
            }
            else
            {
                float orthoSize = cam.GetOrthographicSize();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
                ImGui::TextUnformatted("Size");
                ImGui::PopStyleColor();
                ImGui::SameLine(labelW);
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::DragFloat("##camOrthoSizeCam", &orthoSize, 0.05f, 0.01f, 10000.0f, "%.2f"))
                {
                    cam.SetOrthographicSize(orthoSize);
                    syncViewport();
                }

                float on = cam.GetOrthographicNearClip();
                float of = cam.GetOrthographicFarClip();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
                ImGui::TextUnformatted("Near");
                ImGui::PopStyleColor();
                ImGui::SameLine(labelW);
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::DragFloat("##camONearCam", &on, 0.01f, -10000.0f, of - 0.001f, "%.3f"))
                {
                    cam.SetOrthographicNearClip(on);
                    syncViewport();
                }
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
                ImGui::TextUnformatted("Far");
                ImGui::PopStyleColor();
                ImGui::SameLine(labelW);
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::DragFloat("##camOFarCam", &of, 0.01f, on + 0.001f, 10000.0f, "%.3f"))
                {
                    cam.SetOrthographicFarClip(of);
                    syncViewport();
                }
            }

            syncViewport();
            ImGui::Spacing();
        });

    DrawComponent<CHEngine::RigidBody3DComponent>("RigidBody 3D",
        true,
        propsReadOnly,
        activeSession,
        selectedHandle,
        selectedEntity,
        "RigidBody3D",
        [&](CHEngine::RigidBody3DComponent& rb) {
            const char* bodyTypes[] = { "Static", "Dynamic", "Kinematic" };
            int bt = static_cast<int>(rb.BodyDesc.Type);
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::Combo("##rbBodyTypeRb", &bt, bodyTypes, 3))
                rb.BodyDesc.Type = static_cast<CHEngine::PhysicsBodyType>(bt);

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
            ImGui::TextUnformatted("Mass");
            ImGui::PopStyleColor();
            ImGui::SameLine(labelW);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::DragFloat("##rbMassRb", &rb.BodyDesc.Mass, 0.05f, 0.001f, 100000.0f, "%.3f");

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
            ImGui::TextUnformatted("Linear damp");
            ImGui::PopStyleColor();
            ImGui::SameLine(labelW);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::DragFloat("##rbLinDampRb", &rb.BodyDesc.LinearDamping, 0.01f, 0.0f, 100.0f, "%.3f");

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
            ImGui::TextUnformatted("Angular damp");
            ImGui::PopStyleColor();
            ImGui::SameLine(labelW);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::DragFloat("##rbAngDampRb", &rb.BodyDesc.AngularDamping, 0.01f, 0.0f, 100.0f, "%.3f");

            ImGui::Checkbox("Gravity##rbGravRb", &rb.BodyDesc.EnableGravity);

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
            ImGui::TextUnformatted("Friction (s)");
            ImGui::PopStyleColor();
            ImGui::SameLine(labelW);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::DragFloat("##rbFricSRb", &rb.BodyDesc.StaticFriction, 0.01f, 0.0f, 2.0f, "%.2f");

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
            ImGui::TextUnformatted("Friction (d)");
            ImGui::PopStyleColor();
            ImGui::SameLine(labelW);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::DragFloat("##rbFricDRb", &rb.BodyDesc.DynamicFriction, 0.01f, 0.0f, 2.0f, "%.2f");

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
            ImGui::TextUnformatted("Restitution");
            ImGui::PopStyleColor();
            ImGui::SameLine(labelW);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::DragFloat("##rbRestRb", &rb.BodyDesc.Restitution, 0.01f, 0.0f, 2.0f, "%.2f");

            const char* syncModes[] = { "Auto", "Read from physics", "Write to physics", "Read / write" };
            int sm = static_cast<int>(rb.SyncMode);
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::Combo("##rbSyncRb", &sm, syncModes, 4))
                rb.SyncMode = static_cast<CHEngine::RigidBodySyncMode>(sm);

            const char* shapeTypes[] = { "Box", "Sphere", "Capsule" };
            int st = static_cast<int>(rb.ShapeDesc.Type);
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::Combo("##rbShapeRb", &st, shapeTypes, 3))
                rb.ShapeDesc.Type = static_cast<CHEngine::PhysicsColliderShapeType>(st);

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
            ImGui::TextUnformatted("Half extents");
            ImGui::PopStyleColor();
            ImGui::SameLine(labelW);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::DragFloat3("##rbHalfExtRb", glm::value_ptr(rb.ShapeDesc.HalfExtents), 0.01f, 0.001f, 1000.0f);

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
            ImGui::TextUnformatted("Radius");
            ImGui::PopStyleColor();
            ImGui::SameLine(labelW);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::DragFloat("##rbRadiusRb", &rb.ShapeDesc.Radius, 0.01f, 0.001f, 1000.0f, "%.3f");

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
            ImGui::TextUnformatted("Half height");
            ImGui::PopStyleColor();
            ImGui::SameLine(labelW);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::DragFloat("##rbHalfHRb", &rb.ShapeDesc.HalfHeight, 0.01f, 0.001f, 1000.0f, "%.3f");

            ImGui::Spacing();
        });

    DrawComponent<CHEngine::LifetimeComponent>("Lifetime",
        true,
        propsReadOnly,
        activeSession,
        selectedHandle,
        selectedEntity,
        "Lifetime",
        [&](CHEngine::LifetimeComponent& life) {
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::DragFloat("##lifeRemainLf", &life.RemainingSeconds, 0.05f, 0.0f, 1.0e6f, "%.2f s");
            ImGui::Checkbox("Destroy on expire##lifeDestLf", &life.DestroyOnExpire);
            ImGui::Spacing();
        });

    UIActive::SectionHeader("INFO");
    if (selectedEntity->HasComponent<CHEngine::MeshComponent>())
    {
        auto& meshComp = selectedEntity->GetComponent<CHEngine::MeshComponent>();
        uint32_t tv = 0, ti = 0;
        for (auto& m : meshComp.Meshes)
        {
            tv += static_cast<uint32_t>(m.GetVertices().size());
            ti += static_cast<uint32_t>(m.GetIndices().size());
        }
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
        ImGui::Text("Meshes      %zu", meshComp.Meshes.size());
        ImGui::Text("Vertices    %u", tv);
        ImGui::Text("Triangles   %u", ti / 3);
        ImGui::PopStyleColor();
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
        ImGui::TextUnformatted("No mesh component");
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    if (ImGui::Button("Focus Camera  (F)", ImVec2(-1.0f, 0.0f)))
    {
        host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
            [&host] { host.FocusOnSelected(); }, [] {}, false));
    }

    if (propsReadOnly)
        ImGui::EndDisabled();

    UIActive::EndPanel();
}

} // namespace Sandbox
