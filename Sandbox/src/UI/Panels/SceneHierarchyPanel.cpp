#include "SceneHierarchyPanel.h"

#include "SceneSession.h"
#include "SetTransformCommand.h"

#include <CHEngine/Utils/FileDialog.h>
#include <CHEngine/Scene/Components.h>

#include "UIThemeActive.h"

#include <boost/container_hash/hash.hpp>
#include <optional>

namespace Sandbox {

void SceneHierarchyPanel::Draw(EditorUiHost& host, ImVec2 pos, ImVec2 size, bool reset_layout)
{
    UIActive::BeginPanel("Scene", pos, size, 0, reset_layout);
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

    ImGui::BeginDisabled(activeSession.SessionState != SceneSession::State::Edit);
    if (UIActive::PrimaryButton("+ Import Model", ImVec2(-1.0f, 0.0f)))
    {
        std::string path = CHEngine::FileDialog::OpenModelFile();
        if (!path.empty())
        {
            host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
                [&host, path] { host.ImportModel(path); }, [] {}, false));
        }
    }
    if (ImGui::Button("+ Empty Entity", ImVec2(-1.0f, 0.0f)))
        host.AddEmptyEntity();
    ImGui::EndDisabled();

    ImGui::Spacing();

    float halfW = (ImGui::GetContentRegionAvail().x - 4.0f) * 0.5f;
    if (ImGui::Button("+ Dir Light", ImVec2(halfW, 0.0f)))
    {
        host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
            [&host] { host.AddDirectionalLight(); }, [] {}, false));
    }
    ImGui::SameLine(0, 4);
    if (ImGui::Button("+ Point Light", ImVec2(halfW, 0.0f)))
    {
        host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
            [&host] { host.AddPointLight(); }, [] {}, false));
    }
    if (ImGui::Button("+ Spot Light", ImVec2(-1.0f, 0.0f)))
    {
        host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
            [&host] { host.AddSpotLight(); }, [] {}, false));
    }

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.282f, 0.282f, 0.290f, 0.45f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    std::optional<CHEngine::UUID> deleteID;
    size_t objectCount = 0;
    scene_ptr->ForEach<CHEngine::TagComponent>([&](CHEngine::EntityHandle handle, const CHEngine::UUID& objectID,
                                                CHEngine::TagComponent& tag) {
        ++objectCount;
        bool isSelected = (handle == activeSession.SelectedEntity);
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth
            | ImGuiTreeNodeFlags_FramePadding;
        if (isSelected)
            flags |= ImGuiTreeNodeFlags_Selected;

        const char* icon = "";
        if (const auto* entity = scene_ptr->TryGetEntity(handle);
            entity && entity->HasComponent<CHEngine::LightComponent>())
        {
            const auto type = entity->GetComponent<CHEngine::LightComponent>().LightData.Type;
            if (type == CHEngine::LightType::Directional)
                icon = "[D] ";
            else if (type == CHEngine::LightType::Point)
                icon = "[P] ";
            else if (type == CHEngine::LightType::Spot)
                icon = "[S] ";
        }

        ImGui::PushID(static_cast<int>(boost::hash<CHEngine::UUID>{}(objectID)));
        bool opened = ImGui::TreeNodeEx("##object", flags, "  %s%s", icon, tag.Name.c_str());

        if (ImGui::IsItemClicked())
            host.SetSelection(handle);

        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Focus (F)"))
            {
                host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
                    [&host] { host.FocusOnSelected(); }, [] {}, false));
            }
            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.231f, 0.188f, 1.0f));
            if (ImGui::MenuItem("Delete"))
                deleteID = objectID;
            ImGui::PopStyleColor();
            ImGui::EndPopup();
        }
        if (opened)
            ImGui::TreePop();
        ImGui::PopID();
    });

    if (objectCount == 0)
    {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.686f, 0.686f, 0.706f, 1.0f));
        ImGui::TextWrapped("No objects in scene.\nUse \"+ Empty Entity\" or \"+ Import Model\" to begin.");
        ImGui::PopStyleColor();
    }

    if (ImGui::BeginPopupContextWindow("scene_hierarchy_ctx",
            ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
    {
        if (activeSession.SessionState == SceneSession::State::Edit)
        {
            if (ImGui::MenuItem("Create empty entity"))
                host.AddEmptyEntity();
        }
        ImGui::EndPopup();
    }

    if (deleteID.has_value())
    {
        host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
            [&host, id = *deleteID] { host.DestroyEntityByUuid(id); }, [] {}, false));
    }

    UIActive::EndPanel();
}

} // namespace Sandbox
