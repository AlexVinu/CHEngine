#include "PropertiesPanel.h"

#include "EditorViewport.h"
#include "SceneSession.h"
#include "SetTransformCommand.h"

#include <CHEngine/Utils/FileDialog.h>
#include <CHEngine/Mesh/Material.h>
#include <CHEngine/Render/RenderFacade.h>
#include <CHEngine/Scene/Components.h>

#include "UIThemeActive.h"

#include <glm/gtc/type_ptr.hpp>

#include <cfloat>
#include <cstring>
#include <memory>

namespace Sandbox {

void PropertiesPanel::Draw(EditorUiHost& host, ImVec2 pos, ImVec2 size, bool reset_layout)
{
    UIActive::BeginPanel("Properties", pos, size, 0, reset_layout);
    SceneSession& activeSession = host.GetActiveSceneSession();

    CHEngine::Scene* scenePtr = activeSession.ActiveScene ? activeSession.ActiveScene.get() : activeSession.EditorScene.get();
    if (!scenePtr)
    {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.686f, 0.686f, 0.706f, 1.0f));
        ImGui::TextUnformatted("No scene loaded.");
        ImGui::PopStyleColor();
        UIActive::EndPanel();
        return;
    }
    CHEngine::Scene& scene = *scenePtr;

    const bool hideEditorCameraInUI = (activeSession.SessionState == SceneSession::State::Edit)
        && scene.IsEntityHandleValid(host.GetEditorCameraEntity());
    if (hideEditorCameraInUI && activeSession.SelectedEntity == host.GetEditorCameraEntity())
        activeSession.SelectedEntity = {};

    const CHEngine::EntityHandle selectedHandle = activeSession.SelectedEntity;
    if (!scene.IsEntityHandleValid(selectedHandle))
    {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.686f, 0.686f, 0.706f, 1.0f));
        ImGui::TextUnformatted("No object selected");
        ImGui::PopStyleColor();
        UIActive::EndPanel();
        return;
    }

    auto* selectedEntity = scene.TryGetEntity(selectedHandle);
    if (!selectedEntity || !selectedEntity->HasComponent<CHEngine::TagComponent>()
        || !selectedEntity->HasComponent<CHEngine::TransformComponent>()
        || !selectedEntity->HasComponent<CHEngine::MeshComponent>()
        || !selectedEntity->HasComponent<CHEngine::ColorComponent>()
        || !selectedEntity->HasComponent<CHEngine::VisibilityComponent>())
    {
        UIActive::EndPanel();
        return;
    }
    auto& tag = selectedEntity->GetComponent<CHEngine::TagComponent>();
    auto& transform = selectedEntity->GetComponent<CHEngine::TransformComponent>().ObjectTransform;
    auto& meshComp = selectedEntity->GetComponent<CHEngine::MeshComponent>();
    auto& colorComp = selectedEntity->GetComponent<CHEngine::ColorComponent>();
    auto& visibility = selectedEntity->GetComponent<CHEngine::VisibilityComponent>();

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

    char nameBuf[256];
    std::strncpy(nameBuf, tag.Name.c_str(), sizeof(nameBuf));
    nameBuf[sizeof(nameBuf) - 1] = '\0';
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputText("##name", nameBuf, sizeof(nameBuf)))
        tag.Name = nameBuf;

    UIActive::SectionHeader("TRANSFORM");

    const float labelW = 64.0f;
    auto row = [&](const char* label, const char* id, glm::vec3& vec, float speed, float mn = -FLT_MAX,
                   float mx = FLT_MAX) {
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
                &scene, selectedHandle, host.GetTransformBeforeDrag(), after));
        }
    };
    row("Position", "##pos", transform.Position, 0.05f);
    row("Rotation", "##rot", transform.Rotation, 0.5f);
    row("Scale", "##scl", transform.Scale, 0.01f, 0.001f, 1000.0f);

    ImGui::Spacing();
    if (ImGui::Button("Reset Transform", ImVec2(-1.0f, 0.0f)))
    {
        CHEngine::Transform before = transform;
        host.GetCommandStack().Push(CHEngine::MakeScope<SetTransformCommand>(
            &scene, selectedHandle, before, CHEngine::Transform{}));
    }

    UIActive::SectionHeader("MATERIAL");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::ColorEdit4("##color", glm::value_ptr(colorComp.Color));
    ImGui::Spacing();
    {
        bool before = visibility.Visible;
        if (UIActive::Toggle("Visible", &visibility.Visible) && visibility.Visible != before)
        {
            host.GetCommandStack().Push(CHEngine::MakeScope<SetVisibilityCommand>(
                &scene, scene.GetUUID(selectedHandle), before, visibility.Visible));
        }
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
        CHEngine::MaterialInstance& mat = *subMesh.Mat;

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
        ImGui::TextUnformatted("Textures");
        ImGui::PopStyleColor();

        {
            char diffBuf[512];
            std::string effDiff = mat.EffectiveDiffuseMapPath();
            const char* dSrc = effDiff.empty() ? "(none)" : effDiff.c_str();
            std::strncpy(diffBuf, dSrc, sizeof(diffBuf));
            diffBuf[sizeof(diffBuf) - 1] = '\0';

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
            ImGui::TextUnformatted("Diffuse");
            ImGui::PopStyleColor();
            ImGui::SameLine(labelW);
            float fieldW = ImGui::GetContentRegionAvail().x - 52.0f;
            ImGui::SetNextItemWidth(fieldW);
            ImGui::InputText("##diffPath", diffBuf, sizeof(diffBuf), ImGuiInputTextFlags_ReadOnly);
            ImGui::SameLine(0, 4);
            if (ImGui::Button("...##dif", ImVec2(24, 0)))
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
            if (ImGui::Button("X##dif", ImVec2(20, 0)))
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
            std::string effSpec = mat.EffectiveSpecularMapPath();
            const char* sSrc = effSpec.empty() ? "(none)" : effSpec.c_str();
            std::strncpy(specBuf, sSrc, sizeof(specBuf));
            specBuf[sizeof(specBuf) - 1] = '\0';

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
            ImGui::TextUnformatted("Specular");
            ImGui::PopStyleColor();
            ImGui::SameLine(labelW);
            float fieldW = ImGui::GetContentRegionAvail().x - 52.0f;
            ImGui::SetNextItemWidth(fieldW);
            ImGui::InputText("##specPath", specBuf, sizeof(specBuf), ImGuiInputTextFlags_ReadOnly);
            ImGui::SameLine(0, 4);
            if (ImGui::Button("...##spec", ImVec2(24, 0)))
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
            if (ImGui::Button("X##spec", ImVec2(20, 0)))
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
        ImGui::DragFloat("##shin", &mat.Shininess, 0.5f, 1.0f, 256.0f, "%.1f");

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
        ImGui::TextUnformatted("Specular scale");
        ImGui::PopStyleColor();
        ImGui::SameLine(labelW);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::DragFloat("##specScale", &mat.SpecularScale, 0.02f, 0.0f, 4.0f, "%.2f");

        ImGui::PopID();
    }

    if (selectedEntity->HasComponent<CHEngine::LightComponent>())
    {
        auto& lightData = selectedEntity->GetComponent<CHEngine::LightComponent>().LightData;
        UIActive::SectionHeader("LIGHT");

        const char* lightTypes[] = { "Directional", "Point", "Spot" };
        int ltIdx = static_cast<int>(lightData.Type);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::Combo("##lightType", &ltIdx, lightTypes, 3))
            lightData.Type = static_cast<CHEngine::LightType>(ltIdx);

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
        ImGui::TextUnformatted("Color");
        ImGui::PopStyleColor();
        ImGui::SameLine(labelW);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::ColorEdit3("##lightColor", glm::value_ptr(lightData.Color));

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
        ImGui::TextUnformatted("Intensity");
        ImGui::PopStyleColor();
        ImGui::SameLine(labelW);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::DragFloat("##lightIntensity", &lightData.Intensity, 0.05f, 0.0f, 100.0f, "%.2f");

        if (lightData.Type != CHEngine::LightType::Directional)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
            ImGui::TextUnformatted("Range");
            ImGui::PopStyleColor();
            ImGui::SameLine(labelW);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::DragFloat("##lightRange", &lightData.Range, 0.1f, 0.1f, 1000.0f, "%.1f");
        }

        if (lightData.Type == CHEngine::LightType::Spot)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
            ImGui::TextUnformatted("Inner");
            ImGui::PopStyleColor();
            ImGui::SameLine(labelW);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::DragFloat("##lightInner", &lightData.InnerCone, 0.5f, 0.0f, 89.0f, "%.1f\xc2\xb0");

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
            ImGui::TextUnformatted("Outer");
            ImGui::PopStyleColor();
            ImGui::SameLine(labelW);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::DragFloat("##lightOuter", &lightData.OuterCone, 0.5f, 0.0f, 89.0f, "%.1f\xc2\xb0");
        }

        ImGui::Spacing();
    }

    UIActive::SectionHeader("INFO");
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
