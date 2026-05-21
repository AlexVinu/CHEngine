#include "PropertiesPanel.h"

#include "EditorViewport.h"
#include "SceneSession.h"
#include "SetTransformCommand.h"

#include <CHEngine/Utils/FileDialog.h>
#include <CHEngine/Mesh/Material.h>
#include <CHEngine/Scene/Components.h>
#include <CHEngine/Scene/Light.h>
#include <CHEngine/Camera/PerspectiveCamera.h>
#include <CHEngine/Camera/OrthographicCamera.h>

#include "UIThemeActive.h"

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <filesystem>
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

void PropertiesPanel::Draw(SceneViewLayerHost& host, ImVec2 pos, ImVec2 size, bool reset_layout)
{
    UIActive::BeginPanel("Properties", pos, size, 0, reset_layout);
    SceneSession* activeSession = host.GetActiveSceneSession();

    auto scene_ptr = activeSession->EditorScene;
    if (!scene_ptr)
    {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.686f, 0.686f, 0.706f, 1.0f));
        ImGui::TextUnformatted("No scene loaded.");
        ImGui::PopStyleColor();
        UIActive::EndPanel();
        return;
    }

    const CHEngine::EntityHandle selectedHandle = activeSession->SelectedEntity;
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
    if (!selectedEntity ||
        (!selectedEntity->HasComponent<CHEngine::TransformComponent>() &&
         !selectedEntity->HasComponent<CHEngine::UIRectTransformComponent>() &&
         !selectedEntity->HasComponent<CHEngine::UIOverlayCanvasComponent>() &&
         !selectedEntity->HasComponent<CHEngine::UIWorldCanvasComponent>()))
    {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.686f, 0.686f, 0.706f, 1.0f));
        ImGui::TextUnformatted("Selected entity has no Transform (invalid or incomplete entity).");
        ImGui::PopStyleColor();
        UIActive::EndPanel();
        return;
    }

    const bool propsReadOnly = (activeSession->GetSessionState() != SceneSession::State::Edit);
    if (propsReadOnly)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.7f, 0.1f, 1.0f));
        ImGui::TextUnformatted(activeSession->GetSessionState() == SceneSession::State::Play ? "\xe2\x96\xb6 Play mode — read only"
                : "\xe2\x8f\xb8 Paused — read only");
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::BeginDisabled(true);
    }

    if (!selectedEntity->HasComponent<CHEngine::TagComponent>())
    {
        const std::string uuidLabel = scene_ptr->GetUUID(selectedHandle);
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
                "Tag", selectedEntity, *activeSession, CHEngine::TagComponent{ "Object" });
            DisplayAddComponentEntry<CHEngine::MeshComponent>("Mesh", selectedEntity, *activeSession);
            DisplayAddComponentEntry<CHEngine::ColorComponent>("Color", selectedEntity, *activeSession);
            DisplayAddComponentEntry<CHEngine::VisibilityComponent>("Visibility", selectedEntity, *activeSession);
            DisplayAddComponentEntry<CHEngine::LightComponent>("Light",
                selectedEntity,
                *activeSession,
                CHEngine::LightComponent{ CHEngine::Light{ CHEngine::LightType::Directional } });
            DisplayAddComponentEntry<CHEngine::CameraComponent>("Camera", selectedEntity, *activeSession);
            DisplayAddComponentEntry<CHEngine::RigidBody3DComponent>("RigidBody 3D", selectedEntity, *activeSession);
            DisplayAddComponentEntry<CHEngine::LifetimeComponent>("Lifetime", selectedEntity, *activeSession);
            DisplayAddComponentEntry<CHEngine::ScriptComponent>("Script", selectedEntity, *activeSession);
            ImGui::EndPopup();
        }
        ImGui::Spacing();
    }

    const float labelW = 64.0f;

    DrawComponent<CHEngine::TagComponent>("Tag",
        false,
        propsReadOnly,
        *activeSession,
        selectedHandle,
        selectedEntity,
        "Tag",
        [&](CHEngine::TagComponent& tag) {
            char nameBuf[256];
            std::strncpy(nameBuf, tag.Name.c_str(), sizeof(nameBuf));
            nameBuf[sizeof(nameBuf) - 1] = '\0';
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::InputText("##nameTagComp", nameBuf, sizeof(nameBuf)) && tag.Name != nameBuf)
            {
                selectedEntity->PatchComponent<CHEngine::TagComponent>(
                    [&](CHEngine::TagComponent& tag_component) { tag_component.Name = nameBuf; });
            }
        });

    DrawComponent<CHEngine::TransformComponent>("Transform",
        false,
        propsReadOnly,
        *activeSession,
        selectedHandle,
        selectedEntity,
        "Transform",
        [&](CHEngine::TransformComponent& tc) {
            auto row = [&](const char* label, const char* id, glm::vec3& vec, float speed, float mn = -FLT_MAX, float mx = FLT_MAX) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
                ImGui::TextUnformatted(label);
                ImGui::PopStyleColor();
                ImGui::SameLine(labelW);
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::DragFloat3(id, glm::value_ptr(vec), speed, mn, mx))
                {
                    selectedEntity->PatchComponent<CHEngine::TransformComponent>(
                        [&](CHEngine::TransformComponent& transform_component) {
                            transform_component.ObjectTransform = tc.ObjectTransform;
                            if (std::strcmp(id, "##posTr") == 0)
                                transform_component.ObjectTransform.Position = vec;
                            else if (std::strcmp(id, "##rotTr") == 0)
                                transform_component.ObjectTransform.Rotation = vec;
                            else if (std::strcmp(id, "##sclTr") == 0)
                                transform_component.ObjectTransform.Scale = vec;
                        });
                }
                if (ImGui::IsItemActivated())
                    host.GetTransformBeforeDrag() = tc.ObjectTransform;
                if (ImGui::IsItemDeactivatedAfterEdit())
                {
                    CHEngine::Transform after = selectedEntity->GetComponent<CHEngine::TransformComponent>().ObjectTransform;
                    host.GetCommandStack().Push(MakeScope<SetTransformCommand>(
                        scene_ptr, selectedHandle, host.GetTransformBeforeDrag(), after));
                }
            };
            glm::vec3 position = tc.ObjectTransform.Position;
            glm::vec3 rotation = tc.ObjectTransform.Rotation;
            glm::vec3 scale = tc.ObjectTransform.Scale;
            row("Position", "##posTr", position, 0.05f);
            row("Rotation", "##rotTr", rotation, 0.5f);
            row("Scale", "##sclTr", scale, 0.01f, 0.001f, 1000.0f);

            ImGui::Spacing();
            if (ImGui::Button("Reset Transform##resetTr", ImVec2(-1.0f, 0.0f)))
            {
                CHEngine::Transform before = tc.ObjectTransform;
                host.GetCommandStack().Push(MakeScope<SetTransformCommand>(
                    scene_ptr, selectedHandle, before, CHEngine::Transform{}));
            }
        });

    DrawComponent<CHEngine::ColorComponent>("Color",
        true,
        propsReadOnly,
        *activeSession,
        selectedHandle,
        selectedEntity,
        "Color",
        [&](CHEngine::ColorComponent& colorComp) {
            glm::vec4 color = colorComp.Color;
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::ColorEdit4("##colorComp", glm::value_ptr(color)))
            {
                selectedEntity->PatchComponent<CHEngine::ColorComponent>(
                    [&](CHEngine::ColorComponent& color_component) { color_component.Color = color; });
            }
            ImGui::Spacing();
        });

    DrawComponent<CHEngine::VisibilityComponent>("Visibility",
        true,
        propsReadOnly,
        *activeSession,
        selectedHandle,
        selectedEntity,
        "Visibility",
        [&](CHEngine::VisibilityComponent& visibility) {
            bool before = visibility.Visible;
            bool visible = visibility.Visible;
            if (UIActive::Toggle("Visible##visComp", &visible) && visible != before)
            {
                selectedEntity->PatchComponent<CHEngine::VisibilityComponent>(
                    [&](CHEngine::VisibilityComponent& visibility_component) { visibility_component.Visible = visible; });
                host.GetCommandStack().Push(MakeScope<SetVisibilityCommand>(
                    scene_ptr, scene_ptr->GetUUID(selectedHandle), before, visible));
            }
            ImGui::Spacing();
        });

    DrawComponent<CHEngine::MeshComponent>("Mesh",
        true,
        propsReadOnly,
        *activeSession,
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
                if (ImGui::InputText("##meshSourcePathMesh", pathBuf, sizeof(pathBuf)) && meshComp.SourcePath != pathBuf)
                {
                    selectedEntity->PatchComponent<CHEngine::MeshComponent>(
                        [&](CHEngine::MeshComponent& mesh_component) { mesh_component.SourcePath = pathBuf; });
                }
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
                {
                    const size_t submeshIndex = mi;
                    selectedEntity->PatchComponent<CHEngine::MeshComponent>([&](CHEngine::MeshComponent& mesh_component) {
                        auto& patchSubMesh = mesh_component.Meshes[submeshIndex];
                        patchSubMesh.Mat = CHEngine::MaterialInstance::FromBase(
                            std::make_shared<CHEngine::Material>(host.GetEditorViewport().GetMeshShader()));
                    });
                }
                auto mat_ref = selectedEntity->GetComponent<CHEngine::MeshComponent>().Meshes[mi].Mat;

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
                            host.GetCommandStack().Push(MakeScope<CallbackCommand>(
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
                        host.GetCommandStack().Push(MakeScope<CallbackCommand>(
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
                            host.GetCommandStack().Push(MakeScope<CallbackCommand>(
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
                        host.GetCommandStack().Push(MakeScope<CallbackCommand>(
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
                float shininess = mat_ref->Shininess;
                if (ImGui::DragFloat("##shinMesh", &shininess, 0.5f, 1.0f, 256.0f, "%.1f"))
                {
                    const size_t submeshIndex = mi;
                    selectedEntity->PatchComponent<CHEngine::MeshComponent>(
                        [&](CHEngine::MeshComponent& mesh_component) {
                            auto& patchSubMesh = mesh_component.Meshes[submeshIndex];
                            if (patchSubMesh.Mat)
                                patchSubMesh.Mat->Shininess = shininess;
                        });
                }

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
                ImGui::TextUnformatted("Specular scale");
                ImGui::PopStyleColor();
                ImGui::SameLine(labelW);
                ImGui::SetNextItemWidth(-1.0f);
                float specularScale = mat_ref->SpecularScale;
                if (ImGui::DragFloat("##specScaleMesh", &specularScale, 0.02f, 0.0f, 4.0f, "%.2f"))
                {
                    const size_t submeshIndex = mi;
                    selectedEntity->PatchComponent<CHEngine::MeshComponent>(
                        [&](CHEngine::MeshComponent& mesh_component) {
                            auto& patchSubMesh = mesh_component.Meshes[submeshIndex];
                            if (patchSubMesh.Mat)
                                patchSubMesh.Mat->SpecularScale = specularScale;
                        });
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("PBR");
                ImGui::Spacing();

                bool usePBR = (mat_ref->GetMaterial()->MaterialFlags & CHEngine::kPBR_EnablePBR) != 0;
                if (ImGui::Checkbox("Enable PBR##pbrEnable", &usePBR))
                {
                    const size_t submeshIndex = mi;
                    selectedEntity->PatchComponent<CHEngine::MeshComponent>(
                        [&](CHEngine::MeshComponent& mesh_component) {
                            auto& patchSubMesh = mesh_component.Meshes[submeshIndex];
                            if (patchSubMesh.Mat)
                            {
                                if (usePBR)
                                    patchSubMesh.Mat->GetMaterial()->MaterialFlags |= CHEngine::kPBR_EnablePBR;
                                else
                                    patchSubMesh.Mat->GetMaterial()->MaterialFlags &= ~CHEngine::kPBR_EnablePBR;
                                patchSubMesh.Mat->GetMaterial()->ShaderHandler = usePBR
                                    ? host.GetEditorViewport().GetPBRShader()
                                    : host.GetEditorViewport().GetMeshShader();
                            }
                        });
                }

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
                ImGui::TextUnformatted("Roughness");
                ImGui::PopStyleColor();
                ImGui::SameLine(labelW);
                ImGui::SetNextItemWidth(-1.0f);
                float roughness = mat_ref->Roughness;
                if (ImGui::DragFloat("##roughnessMesh", &roughness, 0.005f, 0.0f, 1.0f, "%.3f"))
                {
                    const size_t submeshIndex = mi;
                    selectedEntity->PatchComponent<CHEngine::MeshComponent>(
                        [&](CHEngine::MeshComponent& mesh_component) {
                            auto& patchSubMesh = mesh_component.Meshes[submeshIndex];
                            if (patchSubMesh.Mat)
                                patchSubMesh.Mat->Roughness = roughness;
                        });
                }

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
                ImGui::TextUnformatted("Metallic");
                ImGui::PopStyleColor();
                ImGui::SameLine(labelW);
                ImGui::SetNextItemWidth(-1.0f);
                float metallic = mat_ref->Metallic;
                if (ImGui::DragFloat("##metallicMesh", &metallic, 0.005f, 0.0f, 1.0f, "%.3f"))
                {
                    const size_t submeshIndex = mi;
                    selectedEntity->PatchComponent<CHEngine::MeshComponent>(
                        [&](CHEngine::MeshComponent& mesh_component) {
                            auto& patchSubMesh = mesh_component.Meshes[submeshIndex];
                            if (patchSubMesh.Mat)
                                patchSubMesh.Mat->Metallic = metallic;
                        });
                }

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
                ImGui::TextUnformatted("AO");
                ImGui::PopStyleColor();
                ImGui::SameLine(labelW);
                ImGui::SetNextItemWidth(-1.0f);
                float ao = mat_ref->AO;
                if (ImGui::DragFloat("##aoMesh", &ao, 0.005f, 0.0f, 1.0f, "%.3f"))
                {
                    const size_t submeshIndex = mi;
                    selectedEntity->PatchComponent<CHEngine::MeshComponent>(
                        [&](CHEngine::MeshComponent& mesh_component) {
                            auto& patchSubMesh = mesh_component.Meshes[submeshIndex];
                            if (patchSubMesh.Mat)
                                patchSubMesh.Mat->AO = ao;
                        });
                }

                ImGui::PopID();
            }
        });

    DrawComponent<CHEngine::LightComponent>("Light",
        true,
        propsReadOnly,
        *activeSession,
        selectedHandle,
        selectedEntity,
        "Light",
        [&](CHEngine::LightComponent& lightComp) {
            const CHEngine::Light lightData = lightComp.LightData;

            const char* lightTypes[] = { "None", "Directional", "Point", "Spot" };
            int ltIdx = LightTypeToComboIndex(lightData.Type);
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::Combo("##lightTypeLt", &ltIdx, lightTypes, 4))
            {
                const CHEngine::LightType nextType = ComboIndexToLightType(ltIdx);
                if (nextType != lightData.Type)
                {
                    selectedEntity->PatchComponent<CHEngine::LightComponent>([&](CHEngine::LightComponent& light_component) {
                        light_component.LightData.Type = nextType;
                    });
                }
            }

            if (lightData.Type != CHEngine::LightType::None)
            {
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
                ImGui::TextUnformatted("Color");
                ImGui::PopStyleColor();
                ImGui::SameLine(labelW);
                ImGui::SetNextItemWidth(-1.0f);
                glm::vec3 lightColor = lightData.Color;
                if (ImGui::ColorEdit3("##lightColorLt", glm::value_ptr(lightColor)))
                {
                    selectedEntity->PatchComponent<CHEngine::LightComponent>([&](CHEngine::LightComponent& light_component) {
                        light_component.LightData.Color = lightColor;
                    });
                }

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
                ImGui::TextUnformatted("Intensity");
                ImGui::PopStyleColor();
                ImGui::SameLine(labelW);
                ImGui::SetNextItemWidth(-1.0f);
                float intensity = lightData.Intensity;
                if (ImGui::DragFloat("##lightIntensityLt", &intensity, 0.05f, 0.0f, 100.0f, "%.2f"))
                {
                    selectedEntity->PatchComponent<CHEngine::LightComponent>([&](CHEngine::LightComponent& light_component) {
                        light_component.LightData.Intensity = intensity;
                    });
                }

                if (lightData.Type != CHEngine::LightType::Directional)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
                    ImGui::TextUnformatted("Range");
                    ImGui::PopStyleColor();
                    ImGui::SameLine(labelW);
                    ImGui::SetNextItemWidth(-1.0f);
                    float range = lightData.Range;
                    if (ImGui::DragFloat("##lightRangeLt", &range, 0.1f, 0.1f, 1000.0f, "%.1f"))
                    {
                        selectedEntity->PatchComponent<CHEngine::LightComponent>(
                            [&](CHEngine::LightComponent& light_component) { light_component.LightData.Range = range; });
                    }
                }

                if (lightData.Type == CHEngine::LightType::Spot)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
                    ImGui::TextUnformatted("Inner");
                    ImGui::PopStyleColor();
                    ImGui::SameLine(labelW);
                    ImGui::SetNextItemWidth(-1.0f);
                    float innerCone = lightData.InnerCone;
                    if (ImGui::DragFloat("##lightInnerLt", &innerCone, 0.5f, 0.0f, 89.0f, "%.1f\xc2\xb0"))
                    {
                        selectedEntity->PatchComponent<CHEngine::LightComponent>(
                            [&](CHEngine::LightComponent& light_component) {
                                light_component.LightData.InnerCone = innerCone;
                            });
                    }

                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
                    ImGui::TextUnformatted("Outer");
                    ImGui::PopStyleColor();
                    ImGui::SameLine(labelW);
                    ImGui::SetNextItemWidth(-1.0f);
                    float outerCone = lightData.OuterCone;
                    if (ImGui::DragFloat("##lightOuterLt", &outerCone, 0.5f, 0.0f, 89.0f, "%.1f\xc2\xb0"))
                    {
                        selectedEntity->PatchComponent<CHEngine::LightComponent>(
                            [&](CHEngine::LightComponent& light_component) {
                                light_component.LightData.OuterCone = outerCone;
                            });
                    }
                }

                ImGui::Spacing();
            }
        });

    DrawComponent<CHEngine::CameraComponent>("Camera",
        true,
        propsReadOnly,
        *activeSession,
        selectedHandle,
        selectedEntity,
        "Camera",
        [&](CHEngine::CameraComponent& camComp) {
            const uint32_t vw = std::max(1u, static_cast<uint32_t>(activeSession->ViewportSize.x));
            const uint32_t vh = std::max(1u, static_cast<uint32_t>(activeSession->ViewportSize.y));
            auto syncViewport = [&] {
                selectedEntity->PatchComponent<CHEngine::CameraComponent>([&](CHEngine::CameraComponent& camera_component) {
                    CHEngine::CameraHelpers::SetViewportSize(camera_component.Camera, vw, vh);
                });
            };

            bool primary = camComp.Primary;
            if (ImGui::Checkbox("Primary##camPrim", &primary))
            {
                selectedEntity->PatchComponent<CHEngine::CameraComponent>([&](CHEngine::CameraComponent& camera_component) {
                    camera_component.Primary = primary;
                    CHEngine::CameraHelpers::SetViewportSize(camera_component.Camera, vw, vh);
                });
                syncViewport();
            }
            bool fixedAspectRatio = camComp.FixedAspectRatio;
            if (ImGui::Checkbox("Fixed aspect ratio##camFixAsp", &fixedAspectRatio))
            {
                selectedEntity->PatchComponent<CHEngine::CameraComponent>([&](CHEngine::CameraComponent& camera_component) {
                    camera_component.FixedAspectRatio = fixedAspectRatio;
                });
            }

            const char* projNames[] = { "Perspective", "Orthographic" };
            int projIdx = std::holds_alternative<CHEngine::PerspectiveCamera>(camComp.Camera) ? 0 : 1;
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::Combo("##camProjCam", &projIdx, projNames, 2))
            {
                selectedEntity->PatchComponent<CHEngine::CameraComponent>(
                    [&](CHEngine::CameraComponent& camera_component) {
                        if (projIdx == 0)
                            camera_component.Camera = CHEngine::PerspectiveCamera{};
                        else
                            camera_component.Camera = CHEngine::OrthographicCamera{};
                        CHEngine::CameraHelpers::SetViewportSize(camera_component.Camera, vw, vh);
                    });
                syncViewport();
            }

            if (std::holds_alternative<CHEngine::PerspectiveCamera>(camComp.Camera))
            {
                const auto& cam = std::get<CHEngine::PerspectiveCamera>(camComp.Camera);
                float fovDeg = glm::degrees(cam.GetVerticalFOV());
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
                ImGui::TextUnformatted("FOV (deg)");
                ImGui::PopStyleColor();
                ImGui::SameLine(labelW);
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::DragFloat("##camFovCam", &fovDeg, 0.25f, 1.0f, 120.0f, "%.1f"))
                {
                    selectedEntity->PatchComponent<CHEngine::CameraComponent>(
                        [&](CHEngine::CameraComponent& camera_component) {
                            std::get<CHEngine::PerspectiveCamera>(camera_component.Camera)
                                .SetVerticalFOV(glm::radians(fovDeg));
                            CHEngine::CameraHelpers::SetViewportSize(camera_component.Camera, vw, vh);
                        });
                    syncViewport();
                }

                float pn = cam.GetNearClip();
                float pf = cam.GetFarClip();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
                ImGui::TextUnformatted("Near");
                ImGui::PopStyleColor();
                ImGui::SameLine(labelW);
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::DragFloat("##camPNearCam", &pn, 0.01f, 0.001f, pf * 0.999f, "%.3f"))
                {
                    selectedEntity->PatchComponent<CHEngine::CameraComponent>(
                        [&](CHEngine::CameraComponent& camera_component) {
                            std::get<CHEngine::PerspectiveCamera>(camera_component.Camera).SetNearClip(pn);
                            CHEngine::CameraHelpers::SetViewportSize(camera_component.Camera, vw, vh);
                        });
                    syncViewport();
                }
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
                ImGui::TextUnformatted("Far");
                ImGui::PopStyleColor();
                ImGui::SameLine(labelW);
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::DragFloat("##camPFarCam", &pf, 1.0f, pn * 1.001f, 100000.0f, "%.1f"))
                {
                    selectedEntity->PatchComponent<CHEngine::CameraComponent>(
                        [&](CHEngine::CameraComponent& camera_component) {
                            std::get<CHEngine::PerspectiveCamera>(camera_component.Camera).SetFarClip(pf);
                            CHEngine::CameraHelpers::SetViewportSize(camera_component.Camera, vw, vh);
                        });
                    syncViewport();
                }
            }
            else
            {
                const auto& cam = std::get<CHEngine::OrthographicCamera>(camComp.Camera);
                float orthoSize = cam.GetSize();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
                ImGui::TextUnformatted("Size");
                ImGui::PopStyleColor();
                ImGui::SameLine(labelW);
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::DragFloat("##camOrthoSizeCam", &orthoSize, 0.05f, 0.01f, 10000.0f, "%.2f"))
                {
                    selectedEntity->PatchComponent<CHEngine::CameraComponent>(
                        [&](CHEngine::CameraComponent& camera_component) {
                            std::get<CHEngine::OrthographicCamera>(camera_component.Camera).SetSize(orthoSize);
                            CHEngine::CameraHelpers::SetViewportSize(camera_component.Camera, vw, vh);
                        });
                    syncViewport();
                }

                float on = cam.GetNearClip();
                float of = cam.GetFarClip();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
                ImGui::TextUnformatted("Near");
                ImGui::PopStyleColor();
                ImGui::SameLine(labelW);
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::DragFloat("##camONearCam", &on, 0.01f, -10000.0f, of - 0.001f, "%.3f"))
                {
                    selectedEntity->PatchComponent<CHEngine::CameraComponent>(
                        [&](CHEngine::CameraComponent& camera_component) {
                            std::get<CHEngine::OrthographicCamera>(camera_component.Camera).SetNearClip(on);
                            CHEngine::CameraHelpers::SetViewportSize(camera_component.Camera, vw, vh);
                        });
                    syncViewport();
                }
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
                ImGui::TextUnformatted("Far");
                ImGui::PopStyleColor();
                ImGui::SameLine(labelW);
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::DragFloat("##camOFarCam", &of, 0.01f, on + 0.001f, 10000.0f, "%.3f"))
                {
                    selectedEntity->PatchComponent<CHEngine::CameraComponent>(
                        [&](CHEngine::CameraComponent& camera_component) {
                            std::get<CHEngine::OrthographicCamera>(camera_component.Camera).SetFarClip(of);
                            CHEngine::CameraHelpers::SetViewportSize(camera_component.Camera, vw, vh);
                        });
                    syncViewport();
                }
            }

            syncViewport();
            ImGui::Spacing();
        });

    DrawComponent<CHEngine::RigidBody3DComponent>("RigidBody 3D",
        true,
        propsReadOnly,
        *activeSession,
        selectedHandle,
        selectedEntity,
        "RigidBody3D",
        [&](CHEngine::RigidBody3DComponent& rb) {
            const char* bodyTypes[] = { "Static", "Dynamic", "Kinematic" };
            int bt = static_cast<int>(rb.BodyDesc.Type);
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::Combo("##rbBodyTypeRb", &bt, bodyTypes, 3))
            {
                selectedEntity->PatchComponent<CHEngine::RigidBody3DComponent>(
                    [&](CHEngine::RigidBody3DComponent& rigidbody_component) {
                        rigidbody_component.BodyDesc.Type = static_cast<CHEngine::PhysicsBodyType>(bt);
                    });
            }

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
            ImGui::TextUnformatted("Mass");
            ImGui::PopStyleColor();
            ImGui::SameLine(labelW);
            ImGui::SetNextItemWidth(-1.0f);
            float mass = rb.BodyDesc.Mass;
            if (ImGui::DragFloat("##rbMassRb", &mass, 0.05f, 0.001f, 100000.0f, "%.3f"))
            {
                selectedEntity->PatchComponent<CHEngine::RigidBody3DComponent>(
                    [&](CHEngine::RigidBody3DComponent& rigidbody_component) { rigidbody_component.BodyDesc.Mass = mass; });
            }

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
            ImGui::TextUnformatted("Linear damp");
            ImGui::PopStyleColor();
            ImGui::SameLine(labelW);
            ImGui::SetNextItemWidth(-1.0f);
            float linearDamping = rb.BodyDesc.LinearDamping;
            if (ImGui::DragFloat("##rbLinDampRb", &linearDamping, 0.01f, 0.0f, 100.0f, "%.3f"))
            {
                selectedEntity->PatchComponent<CHEngine::RigidBody3DComponent>(
                    [&](CHEngine::RigidBody3DComponent& rigidbody_component) {
                        rigidbody_component.BodyDesc.LinearDamping = linearDamping;
                    });
            }

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
            ImGui::TextUnformatted("Angular damp");
            ImGui::PopStyleColor();
            ImGui::SameLine(labelW);
            ImGui::SetNextItemWidth(-1.0f);
            float angularDamping = rb.BodyDesc.AngularDamping;
            if (ImGui::DragFloat("##rbAngDampRb", &angularDamping, 0.01f, 0.0f, 100.0f, "%.3f"))
            {
                selectedEntity->PatchComponent<CHEngine::RigidBody3DComponent>(
                    [&](CHEngine::RigidBody3DComponent& rigidbody_component) {
                        rigidbody_component.BodyDesc.AngularDamping = angularDamping;
                    });
            }

            bool enableGravity = rb.BodyDesc.EnableGravity;
            if (ImGui::Checkbox("Gravity##rbGravRb", &enableGravity))
            {
                selectedEntity->PatchComponent<CHEngine::RigidBody3DComponent>(
                    [&](CHEngine::RigidBody3DComponent& rigidbody_component) {
                        rigidbody_component.BodyDesc.EnableGravity = enableGravity;
                    });
            }

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
            ImGui::TextUnformatted("Friction (s)");
            ImGui::PopStyleColor();
            ImGui::SameLine(labelW);
            ImGui::SetNextItemWidth(-1.0f);
            float staticFriction = rb.BodyDesc.StaticFriction;
            if (ImGui::DragFloat("##rbFricSRb", &staticFriction, 0.01f, 0.0f, 2.0f, "%.2f"))
            {
                selectedEntity->PatchComponent<CHEngine::RigidBody3DComponent>(
                    [&](CHEngine::RigidBody3DComponent& rigidbody_component) {
                        rigidbody_component.BodyDesc.StaticFriction = staticFriction;
                    });
            }

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
            ImGui::TextUnformatted("Friction (d)");
            ImGui::PopStyleColor();
            ImGui::SameLine(labelW);
            ImGui::SetNextItemWidth(-1.0f);
            float dynamicFriction = rb.BodyDesc.DynamicFriction;
            if (ImGui::DragFloat("##rbFricDRb", &dynamicFriction, 0.01f, 0.0f, 2.0f, "%.2f"))
            {
                selectedEntity->PatchComponent<CHEngine::RigidBody3DComponent>(
                    [&](CHEngine::RigidBody3DComponent& rigidbody_component) {
                        rigidbody_component.BodyDesc.DynamicFriction = dynamicFriction;
                    });
            }

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
            ImGui::TextUnformatted("Restitution");
            ImGui::PopStyleColor();
            ImGui::SameLine(labelW);
            ImGui::SetNextItemWidth(-1.0f);
            float restitution = rb.BodyDesc.Restitution;
            if (ImGui::DragFloat("##rbRestRb", &restitution, 0.01f, 0.0f, 2.0f, "%.2f"))
            {
                selectedEntity->PatchComponent<CHEngine::RigidBody3DComponent>(
                    [&](CHEngine::RigidBody3DComponent& rigidbody_component) {
                        rigidbody_component.BodyDesc.Restitution = restitution;
                    });
            }

            const char* syncModes[] = { "Auto", "Read from physics", "Write to physics", "Read / write" };
            int sm = static_cast<int>(rb.SyncMode);
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::Combo("##rbSyncRb", &sm, syncModes, 4))
            {
                selectedEntity->PatchComponent<CHEngine::RigidBody3DComponent>(
                    [&](CHEngine::RigidBody3DComponent& rigidbody_component) {
                        rigidbody_component.SyncMode = static_cast<CHEngine::RigidBodySyncMode>(sm);
                    });
            }

            const char* shapeTypes[] = { "Box", "Sphere", "Capsule" };
            int st = static_cast<int>(rb.ShapeDesc.Type);
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::Combo("##rbShapeRb", &st, shapeTypes, 3))
            {
                selectedEntity->PatchComponent<CHEngine::RigidBody3DComponent>(
                    [&](CHEngine::RigidBody3DComponent& rigidbody_component) {
                        rigidbody_component.ShapeDesc.Type = static_cast<CHEngine::PhysicsColliderShapeType>(st);
                    });
            }

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
            ImGui::TextUnformatted("Half extents");
            ImGui::PopStyleColor();
            ImGui::SameLine(labelW);
            ImGui::SetNextItemWidth(-1.0f);
            glm::vec3 halfExtents = rb.ShapeDesc.HalfExtents;
            if (ImGui::DragFloat3("##rbHalfExtRb", glm::value_ptr(halfExtents), 0.01f, 0.001f, 1000.0f))
            {
                selectedEntity->PatchComponent<CHEngine::RigidBody3DComponent>(
                    [&](CHEngine::RigidBody3DComponent& rigidbody_component) {
                        rigidbody_component.ShapeDesc.HalfExtents = halfExtents;
                    });
            }

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
            ImGui::TextUnformatted("Radius");
            ImGui::PopStyleColor();
            ImGui::SameLine(labelW);
            ImGui::SetNextItemWidth(-1.0f);
            float radius = rb.ShapeDesc.Radius;
            if (ImGui::DragFloat("##rbRadiusRb", &radius, 0.01f, 0.001f, 1000.0f, "%.3f"))
            {
                selectedEntity->PatchComponent<CHEngine::RigidBody3DComponent>(
                    [&](CHEngine::RigidBody3DComponent& rigidbody_component) {
                        rigidbody_component.ShapeDesc.Radius = radius;
                    });
            }

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
            ImGui::TextUnformatted("Half height");
            ImGui::PopStyleColor();
            ImGui::SameLine(labelW);
            ImGui::SetNextItemWidth(-1.0f);
            float halfHeight = rb.ShapeDesc.HalfHeight;
            if (ImGui::DragFloat("##rbHalfHRb", &halfHeight, 0.01f, 0.001f, 1000.0f, "%.3f"))
            {
                selectedEntity->PatchComponent<CHEngine::RigidBody3DComponent>(
                    [&](CHEngine::RigidBody3DComponent& rigidbody_component) {
                        rigidbody_component.ShapeDesc.HalfHeight = halfHeight;
                    });
            }

            bool synchronisedTransform = rb.SynchronisedTransform;
            if (ImGui::Checkbox("Synchronise transform##rbSyncTransformRb", &synchronisedTransform))
            {
                selectedEntity->PatchComponent<CHEngine::RigidBody3DComponent>(
                    [&](CHEngine::RigidBody3DComponent& rigidbody_component) {
                        rigidbody_component.SynchronisedTransform = synchronisedTransform;
                    });
            }

            ImGui::Spacing();
        });

    DrawComponent<CHEngine::LifetimeComponent>("Lifetime",
        true,
        propsReadOnly,
        *activeSession,
        selectedHandle,
        selectedEntity,
        "Lifetime",
        [&](CHEngine::LifetimeComponent& life) {
            float remainingSeconds = life.RemainingSeconds;
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::DragFloat("##lifeRemainLf", &remainingSeconds, 0.05f, 0.0f, 1.0e6f, "%.2f s"))
            {
                selectedEntity->PatchComponent<CHEngine::LifetimeComponent>(
                    [&](CHEngine::LifetimeComponent& lifetime_component) {
                        lifetime_component.RemainingSeconds = remainingSeconds;
                    });
            }
            bool destroyOnExpire = life.DestroyOnExpire;
            if (ImGui::Checkbox("Destroy on expire##lifeDestLf", &destroyOnExpire))
            {
                selectedEntity->PatchComponent<CHEngine::LifetimeComponent>(
                    [&](CHEngine::LifetimeComponent& lifetime_component) {
                        lifetime_component.DestroyOnExpire = destroyOnExpire;
                    });
            }
            ImGui::Spacing();
        });

    DrawComponent<CHEngine::ScriptComponent>("Scripts",
        true,
        propsReadOnly,
        *activeSession,
        selectedHandle,
        selectedEntity,
        "Script",
        [&](CHEngine::ScriptComponent& script) {
            int removeIdx = -1;
            for (int i = 0; i < static_cast<int>(script.Scripts.size()); ++i)
            {
                const auto& entry = script.Scripts[i];
                ImGui::PushID(i);

                // Filename label
                std::string filename = entry.Path.empty()
                    ? "(no path)"
                    : std::filesystem::path(entry.Path).filename().string();
                ImGui::PushStyleColor(ImGuiCol_Text,
                    entry.Path.empty()
                        ? ImVec4(0.55f, 0.55f, 0.57f, 1.0f)
                        : ImVec4(0.55f, 0.85f, 0.55f, 1.0f));
                ImGui::TextUnformatted(filename.c_str());
                ImGui::PopStyleColor();

                if (!propsReadOnly)
                {
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Edit") && !entry.Path.empty())
                        host.OpenScriptInEditor(entry.Path);

                    ImGui::SameLine();
                    if (ImGui::SmallButton("Browse"))
                    {
                        std::string picked = CHEngine::FileDialog::OpenFile("Lua Script", "*.lua");
                        if (!picked.empty())
                        {
                            int idx = i;
                            selectedEntity->PatchComponent<CHEngine::ScriptComponent>(
                                [idx, picked](CHEngine::ScriptComponent& sc) {
                                    if (idx < static_cast<int>(sc.Scripts.size()))
                                        sc.Scripts[idx].Path = picked;
                                });
                        }
                    }

                    ImGui::SameLine();
                    if (ImGui::SmallButton("x"))
                        removeIdx = i;

                    // Enabled checkbox on next line
                    bool enabled = entry.Enabled;
                    if (ImGui::Checkbox("Enabled", &enabled))
                    {
                        int idx = i;
                        selectedEntity->PatchComponent<CHEngine::ScriptComponent>(
                            [idx, enabled](CHEngine::ScriptComponent& sc) {
                                if (idx < static_cast<int>(sc.Scripts.size()))
                                    sc.Scripts[idx].Enabled = enabled;
                            });
                    }
                }

                ImGui::Separator();
                ImGui::PopID();
            }

            // Apply removal after the loop (avoid invalidating the range)
            if (removeIdx >= 0)
            {
                int idx = removeIdx;
                selectedEntity->PatchComponent<CHEngine::ScriptComponent>(
                    [idx](CHEngine::ScriptComponent& sc) {
                        if (idx < static_cast<int>(sc.Scripts.size()))
                            sc.Scripts.erase(sc.Scripts.begin() + idx);
                    });
            }

            if (script.Scripts.empty())
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.57f, 1.0f));
                ImGui::TextUnformatted("No scripts assigned");
                ImGui::PopStyleColor();
            }

            if (!propsReadOnly)
            {
                ImGui::Spacing();
                if (ImGui::Button("+ Add Script"))
                {
                    std::string tag = selectedEntity->HasComponent<CHEngine::TagComponent>()
                        ? selectedEntity->GetComponent<CHEngine::TagComponent>().Name
                        : "Entity";
                    host.CreateAndAttachScript(selectedHandle, tag);
                }
                ImGui::SameLine();
                if (ImGui::Button("Browse..."))
                {
                    std::string picked = CHEngine::FileDialog::OpenFile("Lua Script", "*.lua");
                    if (!picked.empty())
                    {
                        selectedEntity->PatchComponent<CHEngine::ScriptComponent>(
                            [picked](CHEngine::ScriptComponent& sc) {
                                sc.Scripts.push_back(CHEngine::ScriptEntry{ picked, true });
                            });
                    }
                }
            }
            ImGui::Spacing();
        });

    // ── UI Components ─────────────────────────────────────────────────────────

    DrawComponent<CHEngine::UIOverlayCanvasComponent>("UI Overlay Canvas", true, propsReadOnly,
        *activeSession, selectedHandle, selectedEntity, "UIOverlayCanvas",
        [&](CHEngine::UIOverlayCanvasComponent& c) {
            float pos[2] = { c.Position.x, c.Position.y };
            if (ImGui::DragFloat2("Position##ovc", pos, 1.0f))
                selectedEntity->PatchComponent<CHEngine::UIOverlayCanvasComponent>(
                    [&](CHEngine::UIOverlayCanvasComponent& cc) { cc.Position = { pos[0], pos[1] }; });
            float sz[2] = { c.Size.x, c.Size.y };
            if (ImGui::DragFloat2("Size##ovc", sz, 0.01f, 0.0f, 1.0f, "%.2f"))
                selectedEntity->PatchComponent<CHEngine::UIOverlayCanvasComponent>(
                    [&](CHEngine::UIOverlayCanvasComponent& cc) { cc.Size = { sz[0], sz[1] }; });
            float anc[2] = { c.AnchorMin.x, c.AnchorMin.y };
            if (ImGui::DragFloat2("Anchor##ovc", anc, 0.01f, 0.0f, 1.0f))
                selectedEntity->PatchComponent<CHEngine::UIOverlayCanvasComponent>(
                    [&](CHEngine::UIOverlayCanvasComponent& cc) {
                        cc.AnchorMin = cc.AnchorMax = { anc[0], anc[1] }; });
            float pivot[2] = { c.Pivot.x, c.Pivot.y };
            if (ImGui::DragFloat2("Pivot##ovc", pivot, 0.01f, 0.0f, 1.0f))
                selectedEntity->PatchComponent<CHEngine::UIOverlayCanvasComponent>(
                    [&](CHEngine::UIOverlayCanvasComponent& cc) { cc.Pivot = { pivot[0], pivot[1] }; });
            int so = c.SortOrder;
            if (ImGui::InputInt("Sort Order##ovc", &so))
                selectedEntity->PatchComponent<CHEngine::UIOverlayCanvasComponent>(
                    [so](CHEngine::UIOverlayCanvasComponent& cc) { cc.SortOrder = so; });
            float a = c.Alpha;
            if (ImGui::SliderFloat("Alpha##ovc", &a, 0.0f, 1.0f))
                selectedEntity->PatchComponent<CHEngine::UIOverlayCanvasComponent>(
                    [a](CHEngine::UIOverlayCanvasComponent& cc) { cc.Alpha = a; });
            // ── Children list ─────────────────────────────────────────────────
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextUnformatted("UI Elements");
            ImGui::Spacing();

            CHEngine::UUID canvasUuid = boost::uuids::nil_uuid();
            if (selectedEntity->HasComponent<CHEngine::IDComponent>())
                canvasUuid = selectedEntity->GetComponent<CHEngine::IDComponent>().Value;

            std::vector<CHEngine::EntityHandle> childHandles;
            std::vector<CHEngine::UUID>         childUuids;
            std::vector<std::string>            childNames;
            scene_ptr->ForEach<CHEngine::UIRectTransformComponent, CHEngine::ParentNodeComponent>(
                [&](CHEngine::EntityHandle h, const CHEngine::UUID& uuid,
                    CHEngine::UIRectTransformComponent& rt, CHEngine::ParentNodeComponent& pr)
                {
                    if (pr.Value != canvasUuid) return;
                    std::string nm = "UI Element";
                    auto* ce = scene_ptr->TryGetEntity(h);
                    if (ce && ce->HasComponent<CHEngine::TagComponent>())
                        nm = ce->GetComponent<CHEngine::TagComponent>().Name;
                    childHandles.push_back(h);
                    childUuids.push_back(uuid);
                    childNames.push_back(std::move(nm));
                });

            if (childHandles.empty())
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.57f, 1.0f));
                ImGui::TextUnformatted("No UI elements");
                ImGui::PopStyleColor();
            }
            else
            {
                for (int ci = 0; ci < (int)childHandles.size(); ++ci)
                {
                    bool isSel = (activeSession->SelectedEntity == childHandles[ci]);
                    ImGui::PushID(ci);
                    float avail = ImGui::GetContentRegionAvail().x - 28.0f;
                    if (ImGui::Selectable(childNames[ci].c_str(), isSel,
                                          ImGuiSelectableFlags_None, ImVec2(avail, 0)))
                        host.SetSelection(childHandles[ci]);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("x"))
                        host.DestroyEntityByUuid(childUuids[ci]);
                    ImGui::PopID();
                }
            }
            ImGui::Spacing();
        });

    DrawComponent<CHEngine::UIWorldCanvasComponent>("UI World Canvas", true, propsReadOnly,
        *activeSession, selectedHandle, selectedEntity, "UIWorldCanvas",
        [&](CHEngine::UIWorldCanvasComponent& c) {
            float sz[2] = { c.Size.x, c.Size.y };
            if (ImGui::DragFloat2("Size (m)##wc", sz, 0.05f, 0.01f, 1000.0f))
                selectedEntity->PatchComponent<CHEngine::UIWorldCanvasComponent>(
                    [&](CHEngine::UIWorldCanvasComponent& cc) { cc.Size = { sz[0], sz[1] }; });
            float a = c.Alpha;
            if (ImGui::SliderFloat("Alpha##wc", &a, 0.0f, 1.0f))
                selectedEntity->PatchComponent<CHEngine::UIWorldCanvasComponent>(
                    [a](CHEngine::UIWorldCanvasComponent& cc) { cc.Alpha = a; });
            bool ds = c.DoubleSided;
            if (ImGui::Checkbox("Double Sided##wc", &ds))
                selectedEntity->PatchComponent<CHEngine::UIWorldCanvasComponent>(
                    [ds](CHEngine::UIWorldCanvasComponent& cc) { cc.DoubleSided = ds; });
            // ── Children list ─────────────────────────────────────────────────
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextUnformatted("UI Elements");
            ImGui::Spacing();

            CHEngine::UUID wcanvasUuid = boost::uuids::nil_uuid();
            if (selectedEntity->HasComponent<CHEngine::IDComponent>())
                wcanvasUuid = selectedEntity->GetComponent<CHEngine::IDComponent>().Value;

            std::vector<CHEngine::EntityHandle> wChildHandles;
            std::vector<CHEngine::UUID>         wChildUuids;
            std::vector<std::string>            wChildNames;
            scene_ptr->ForEach<CHEngine::UIRectTransformComponent, CHEngine::ParentNodeComponent>(
				[&](CHEngine::EntityHandle h, const CHEngine::UUID& uuid,
					CHEngine::UIRectTransformComponent& rt, CHEngine::ParentNodeComponent& pr)
				{
					if (pr.Value != wcanvasUuid) return;
                    std::string nm = "UI Element";
                    auto* ce = scene_ptr->TryGetEntity(h);
                    if (ce && ce->HasComponent<CHEngine::TagComponent>())
                        nm = ce->GetComponent<CHEngine::TagComponent>().Name;
                    wChildHandles.push_back(h);
                    wChildUuids.push_back(uuid);
                    wChildNames.push_back(std::move(nm));
                });

            if (wChildHandles.empty())
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.57f, 1.0f));
                ImGui::TextUnformatted("No UI elements");
                ImGui::PopStyleColor();
            }
            else
            {
                for (int ci = 0; ci < (int)wChildHandles.size(); ++ci)
                {
                    bool isSel = (activeSession->SelectedEntity == wChildHandles[ci]);
                    ImGui::PushID(ci + 10000); // offset to avoid ID clash with overlay section
                    float avail = ImGui::GetContentRegionAvail().x - 28.0f;
                    if (ImGui::Selectable(wChildNames[ci].c_str(), isSel,
                                          ImGuiSelectableFlags_None, ImVec2(avail, 0)))
                        host.SetSelection(wChildHandles[ci]);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("x"))
                        host.DestroyEntityByUuid(wChildUuids[ci]);
                    ImGui::PopID();
                }
            }
            ImGui::Spacing();
        });

    DrawComponent<CHEngine::UIRectTransformComponent>("UI Rect Transform", true, propsReadOnly,
        *activeSession, selectedHandle, selectedEntity, "UIRect",
        [&](CHEngine::UIRectTransformComponent& rt) {
            float sz = rt.Size;
            if (ImGui::DragFloat("Size##rt", &sz, 1.0f, 0.0f, 4000.0f))
                selectedEntity->PatchComponent<CHEngine::UIRectTransformComponent>(
                    [sz](CHEngine::UIRectTransformComponent& r) { r.Size = sz; });
            float anc[2] = { rt.AnchorMin.x, rt.AnchorMin.y };
            if (ImGui::DragFloat2("Anchor##rt", anc, 0.01f, 0.0f, 1.0f))
                selectedEntity->PatchComponent<CHEngine::UIRectTransformComponent>(
                    [&](CHEngine::UIRectTransformComponent& r) {
                        r.AnchorMin = r.AnchorMax = { anc[0], anc[1] }; });
            float pivot[2] = { rt.Pivot.x, rt.Pivot.y };
            if (ImGui::DragFloat2("Pivot##rt", pivot, 0.01f, 0.0f, 1.0f))
                selectedEntity->PatchComponent<CHEngine::UIRectTransformComponent>(
                    [&](CHEngine::UIRectTransformComponent& r) { r.Pivot = { pivot[0], pivot[1] }; });
            float alpha = rt.Alpha;
            if (ImGui::SliderFloat("Alpha##rt", &alpha, 0.0f, 1.0f))
                selectedEntity->PatchComponent<CHEngine::UIRectTransformComponent>(
                    [alpha](CHEngine::UIRectTransformComponent& r) { r.Alpha = alpha; });
            ImGui::Spacing();
        });

    DrawComponent<CHEngine::UIPanelComponent>("UI Panel", true, propsReadOnly,
        *activeSession, selectedHandle, selectedEntity, "UIPanel",
        [&](CHEngine::UIPanelComponent& c) {
            float pw = c.Width;
            if (ImGui::DragFloat("Width##panel", &pw, 1.0f, 0.0f, 4000.0f))
                selectedEntity->PatchComponent<CHEngine::UIPanelComponent>(
                    [pw](CHEngine::UIPanelComponent& p) { p.Width = pw; });
            float bcol[4] = { c.BorderColor.r, c.BorderColor.g, c.BorderColor.b, c.BorderColor.a };
            if (ImGui::ColorEdit4("Border Color##panel", bcol))
                selectedEntity->PatchComponent<CHEngine::UIPanelComponent>(
                    [&](CHEngine::UIPanelComponent& p) { p.BorderColor = {bcol[0],bcol[1],bcol[2],bcol[3]}; });
            float bw = c.BorderWidth;
            if (ImGui::DragFloat("Border Width##panel", &bw, 0.5f, 0.0f, 20.0f))
                selectedEntity->PatchComponent<CHEngine::UIPanelComponent>(
                    [bw](CHEngine::UIPanelComponent& p) { p.BorderWidth = bw; });
            float cr = c.CornerRadius;
            if (ImGui::DragFloat("Corner Radius##panel", &cr, 0.5f, 0.0f, 50.0f))
                selectedEntity->PatchComponent<CHEngine::UIPanelComponent>(
                    [cr](CHEngine::UIPanelComponent& p) { p.CornerRadius = cr; });
            ImGui::Spacing();
        });

    DrawComponent<CHEngine::UITextComponent>("UI Text", true, propsReadOnly,
        *activeSession, selectedHandle, selectedEntity, "UIText",
        [&](CHEngine::UITextComponent& c) {
            char buf[1024]; std::strncpy(buf, c.Text.c_str(), sizeof(buf)-1); buf[sizeof(buf)-1]=0;
            if (ImGui::InputTextMultiline("Text##uitext", buf, sizeof(buf), ImVec2(-1, 60)))
                selectedEntity->PatchComponent<CHEngine::UITextComponent>(
                    [&](CHEngine::UITextComponent& t) { t.Text = buf; });
            char fbuf[512]; std::strncpy(fbuf, c.FontPath.c_str(), sizeof(fbuf)-1); fbuf[sizeof(fbuf)-1]=0;
            if (ImGui::InputText("Font Path##uitext", fbuf, sizeof(fbuf)))
                selectedEntity->PatchComponent<CHEngine::UITextComponent>(
                    [&](CHEngine::UITextComponent& t) { t.FontPath = fbuf; });
            float fs = c.FontSize;
            if (ImGui::DragFloat("Font Size##uitext", &fs, 0.5f, 6.0f, 120.0f))
                selectedEntity->PatchComponent<CHEngine::UITextComponent>(
                    [fs](CHEngine::UITextComponent& t) { t.FontSize = fs; });
            float col[4] = { c.Color.r, c.Color.g, c.Color.b, c.Color.a };
            if (ImGui::ColorEdit4("Color##uitext", col))
                selectedEntity->PatchComponent<CHEngine::UITextComponent>(
                    [&](CHEngine::UITextComponent& t) { t.Color = {col[0],col[1],col[2],col[3]}; });
            const char* haligns[] = { "Left", "Center", "Right" };
            int ha = (int)c.HorizontalAlign;
            if (ImGui::Combo("H Align##uitext", &ha, haligns, 3))
                selectedEntity->PatchComponent<CHEngine::UITextComponent>(
                    [ha](CHEngine::UITextComponent& t) { t.HorizontalAlign = (CHEngine::UITextComponent::HAlign)ha; });
            const char* valigns[] = { "Top", "Middle", "Bottom" };
            int va = (int)c.VerticalAlign;
            if (ImGui::Combo("V Align##uitext", &va, valigns, 3))
                selectedEntity->PatchComponent<CHEngine::UITextComponent>(
                    [va](CHEngine::UITextComponent& t) { t.VerticalAlign = (CHEngine::UITextComponent::VAlign)va; });
            bool ww = c.WordWrap;
            if (ImGui::Checkbox("Word Wrap##uitext", &ww))
                selectedEntity->PatchComponent<CHEngine::UITextComponent>(
                    [ww](CHEngine::UITextComponent& t) { t.WordWrap = ww; });
            ImGui::Spacing();
        });

    DrawComponent<CHEngine::UIImageComponent>("UI Image", true, propsReadOnly,
        *activeSession, selectedHandle, selectedEntity, "UIImage",
        [&](CHEngine::UIImageComponent& c) {
            float iw = c.Width;
            if (ImGui::DragFloat("Width##uiimg", &iw, 1.0f, 0.0f, 4000.0f))
                selectedEntity->PatchComponent<CHEngine::UIImageComponent>(
                    [iw](CHEngine::UIImageComponent& i) { i.Width = iw; });
            char tbuf[512]; std::strncpy(tbuf, c.TexturePath.c_str(), sizeof(tbuf)-1); tbuf[sizeof(tbuf)-1]=0;
            if (ImGui::InputText("Texture##uiimg", tbuf, sizeof(tbuf)))
                selectedEntity->PatchComponent<CHEngine::UIImageComponent>(
                    [&](CHEngine::UIImageComponent& i) { i.TexturePath = tbuf; });
            bool pa = c.PreserveAspect;
            if (ImGui::Checkbox("Preserve Aspect##uiimg", &pa))
                selectedEntity->PatchComponent<CHEngine::UIImageComponent>(
                    [pa](CHEngine::UIImageComponent& i) { i.PreserveAspect = pa; });
            ImGui::Spacing();
        });

    DrawComponent<CHEngine::UIButtonComponent>("UI Button", true, propsReadOnly,
        *activeSession, selectedHandle, selectedEntity, "UIButton",
        [&](CHEngine::UIButtonComponent& c) {
            float bw = c.Width;
            if (ImGui::DragFloat("Width##uibtn", &bw, 1.0f, 0.0f, 4000.0f))
                selectedEntity->PatchComponent<CHEngine::UIButtonComponent>(
                    [bw](CHEngine::UIButtonComponent& b) { b.Width = bw; });
            float nc[4]={c.NormalColor.r,c.NormalColor.g,c.NormalColor.b,c.NormalColor.a};
            if (ImGui::ColorEdit4("Normal##uibtn", nc))
                selectedEntity->PatchComponent<CHEngine::UIButtonComponent>(
                    [&](CHEngine::UIButtonComponent& b){ b.NormalColor={nc[0],nc[1],nc[2],nc[3]}; });
            float hc[4]={c.HoverColor.r,c.HoverColor.g,c.HoverColor.b,c.HoverColor.a};
            if (ImGui::ColorEdit4("Hover##uibtn", hc))
                selectedEntity->PatchComponent<CHEngine::UIButtonComponent>(
                    [&](CHEngine::UIButtonComponent& b){ b.HoverColor={hc[0],hc[1],hc[2],hc[3]}; });
            float pc[4]={c.PressedColor.r,c.PressedColor.g,c.PressedColor.b,c.PressedColor.a};
            if (ImGui::ColorEdit4("Pressed##uibtn", pc))
                selectedEntity->PatchComponent<CHEngine::UIButtonComponent>(
                    [&](CHEngine::UIButtonComponent& b){ b.PressedColor={pc[0],pc[1],pc[2],pc[3]}; });
            bool inter = c.Interactable;
            if (ImGui::Checkbox("Interactable##uibtn", &inter))
                selectedEntity->PatchComponent<CHEngine::UIButtonComponent>(
                    [inter](CHEngine::UIButtonComponent& b){ b.Interactable = inter; });
            float cr = c.CornerRadius;
            if (ImGui::DragFloat("Corner Radius##uibtn", &cr, 0.5f, 0.0f, 50.0f))
                selectedEntity->PatchComponent<CHEngine::UIButtonComponent>(
                    [cr](CHEngine::UIButtonComponent& b){ b.CornerRadius = cr; });
            ImGui::Spacing();
        });

    DrawComponent<CHEngine::UISliderComponent>("UI Slider", true, propsReadOnly,
        *activeSession, selectedHandle, selectedEntity, "UISlider",
        [&](CHEngine::UISliderComponent& c) {
            float sw = c.Width;
            if (ImGui::DragFloat("Width##uislider", &sw, 1.0f, 0.0f, 4000.0f))
                selectedEntity->PatchComponent<CHEngine::UISliderComponent>(
                    [sw](CHEngine::UISliderComponent& s) { s.Width = sw; });
            float val = c.Value;
            if (ImGui::SliderFloat("Value##uislider", &val, c.Min, c.Max))
                selectedEntity->PatchComponent<CHEngine::UISliderComponent>(
                    [val](CHEngine::UISliderComponent& s){ s.Value = val; });
            float mn = c.Min, mx = c.Max;
            if (ImGui::DragFloat("Min##uislider", &mn, 0.01f))
                selectedEntity->PatchComponent<CHEngine::UISliderComponent>(
                    [mn](CHEngine::UISliderComponent& s){ s.Min = mn; });
            if (ImGui::DragFloat("Max##uislider", &mx, 0.01f))
                selectedEntity->PatchComponent<CHEngine::UISliderComponent>(
                    [mx](CHEngine::UISliderComponent& s){ s.Max = mx; });
            float fc[4]={c.FillColor.r,c.FillColor.g,c.FillColor.b,c.FillColor.a};
            if (ImGui::ColorEdit4("Fill Color##uislider", fc))
                selectedEntity->PatchComponent<CHEngine::UISliderComponent>(
                    [&](CHEngine::UISliderComponent& s){ s.FillColor={fc[0],fc[1],fc[2],fc[3]}; });
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
        host.GetCommandStack().Push(MakeScope<CallbackCommand>(
            [&host] { host.FocusOnSelected(); }, [] {}, false));
    }

    if (propsReadOnly)
        ImGui::EndDisabled();

    UIActive::EndPanel();
}

} // namespace Sandbox
