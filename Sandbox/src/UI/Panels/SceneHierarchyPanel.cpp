#include "SceneHierarchyPanel.h"

#include "SceneSession.h"
#include "SetTransformCommand.h"

#include <CHEngine/Application.h>
#include <CHEngine/Utils/FileDialog.h>
#include <CHEngine/Scene/Components.h>

#include "UIThemeActive.h"

#include <boost/container_hash/hash.hpp>
#include <cstdio>
#include <optional>

namespace Sandbox {

void SceneHierarchyPanel::EnsureLogo()
{
    if (m_LogoLoaded) return;
    m_LogoLoaded = true;

    const char* path = "editor_assets/icons/logo.png";
    if (FILE* f = std::fopen(path, "rb"))
    {
        uint8_t buf[24] = {};
        if (std::fread(buf, 1, sizeof(buf), f) == sizeof(buf))
        {
            uint32_t w = (buf[16]<<24)|(buf[17]<<16)|(buf[18]<<8)|buf[19];
            uint32_t h = (buf[20]<<24)|(buf[21]<<16)|(buf[22]<<8)|buf[23];
            if (w > 0 && h > 0)
                m_LogoAspect = static_cast<float>(w) / static_cast<float>(h);
        }
        std::fclose(f);
    }
    m_Logo = CHEngine::RenderFacade::CreateTextureFromFile(path);
}

// Draw only the content (no panel wrapper) — used as a tab inside CameraPanel
void SceneHierarchyPanel::DrawContent(SceneViewLayerHost& host)
{
    EnsureLogo();

    Ref<SceneSession> activeSession = host.GetActiveSceneSession();
    auto scene_ptr = activeSession->EditorScene;
    if (!scene_ptr)
    {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.686f, 0.686f, 0.706f, 1.0f));
        ImGui::TextUnformatted("No scene loaded.");
        ImGui::PopStyleColor();
        return;
    }

    // Кнопки добавления объектов убраны — используй Shift+A
    ImGui::Spacing();

    std::optional<CHEngine::UUID> deleteID;
    size_t objectCount = 0;
    scene_ptr->ForEach<CHEngine::TagComponent>(
        [&](CHEngine::EntityHandle handle, const CHEngine::UUID& objectID, CHEngine::TagComponent& tag)
    {
        ++objectCount;
        bool isSelected = (handle == activeSession->SelectedEntity);
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth
                                 | ImGuiTreeNodeFlags_FramePadding;
        if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

        const char* icon = "";
        if (const auto* entity = scene_ptr->TryGetEntity(handle);
            entity && entity->HasComponent<CHEngine::LightComponent>())
        {
            const auto type = entity->GetComponent<CHEngine::LightComponent>().LightData.Type;
            if      (type == CHEngine::LightType::Directional) icon = "[D] ";
            else if (type == CHEngine::LightType::Point)        icon = "[P] ";
            else if (type == CHEngine::LightType::Spot)         icon = "[S] ";
        }

        ImGui::PushID(static_cast<int>(boost::hash<CHEngine::UUID>{}(objectID)));
        bool opened = ImGui::TreeNodeEx("##object", flags, "  %s%s", icon, tag.Name.c_str());
        if (ImGui::IsItemClicked()) host.SetSelection(handle);

        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Focus (F)"))
            {
                host.GetCommandStack().Push(MakeScope<CallbackCommand>(
                    [&host] { host.FocusOnSelected(); }, [] {}, false));
            }
            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.231f, 0.188f, 1.0f));
            if (ImGui::MenuItem("Delete")) deleteID = objectID;
            ImGui::PopStyleColor();
            ImGui::EndPopup();
        }
        if (opened) ImGui::TreePop();
        ImGui::PopID();
    });

    if (objectCount == 0)
    {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.52f, 1.0f));
        ImGui::TextUnformatted("Press Shift+A to add objects");
        ImGui::PopStyleColor();
    }

    if (ImGui::BeginPopupContextWindow("scene_hierarchy_ctx",
            ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
    {
        if (activeSession->GetSessionState() == SceneSession::State::Edit)
        {
            if (ImGui::MenuItem("Create empty entity"))
                host.AddEmptyEntity();
        }
        ImGui::EndPopup();
    }

    if (deleteID.has_value())
    {
        host.GetCommandStack().Push(MakeScope<CallbackCommand>(
            [&host, id = *deleteID] { host.DestroyEntityByUuid(id); }, [] {}, false));
    }
}

void SceneHierarchyPanel::Draw(SceneViewLayerHost& host, ImVec2 pos, ImVec2 size, bool reset_layout)
{
    UIActive::BeginPanel("Scene", pos, size, 0, reset_layout);
    DrawContent(host);
    UIActive::EndPanel();
}

} // namespace Sandbox
