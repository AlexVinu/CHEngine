#include "SceneHierarchyPanel.h"

#include "SceneSession.h"
#include "SetTransformCommand.h"

#include <CHEngine/Application.h>
#include <CHEngine/Utils/FileDialog.h>
#include <CHEngine/Scene/Components.h>

#include "UIThemeActive.h"

#include <cstdio>
#include <filesystem>
#include <functional>
#include <map>
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
    m_Logo = CHEngine::Application::Get().Render().CreateTextureFromFile(path);
}

// Draw only the content (no panel wrapper) — used as a tab inside CameraPanel
void SceneHierarchyPanel::DrawContent(SceneViewLayerHost& host)
{
    EnsureLogo();

    SceneSession* activeSession = host.GetActiveSceneSession();
    auto scene_ptr = activeSession->EditorScene;
    if (!scene_ptr)
    {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.686f, 0.686f, 0.706f, 1.0f));
        ImGui::TextUnformatted("No scene loaded.");
        ImGui::PopStyleColor();
        return;
    }

    // ── World Scripts ─────────────────────────────────────────────────────────
    bool inPlayMode = (activeSession->GetSessionState() != SceneSession::State::Edit);
    if (ImGui::CollapsingHeader("World Scripts"))
    {
        auto& worldScripts = scene_ptr->WorldScripts;
        int removeWsIdx = -1;

        for (int i = 0; i < static_cast<int>(worldScripts.size()); ++i)
        {
            auto& entry = worldScripts[i];
            ImGui::PushID(1000 + i);

            std::string filename = entry.Path.empty()
                ? "(no path)"
                : std::filesystem::path(entry.Path).filename().string();
            ImGui::PushStyleColor(ImGuiCol_Text,
                entry.Path.empty()
                    ? ImVec4(0.55f, 0.55f, 0.57f, 1.0f)
                    : ImVec4(0.55f, 0.85f, 0.55f, 1.0f));
            ImGui::TextUnformatted(filename.c_str());
            ImGui::PopStyleColor();

            if (!inPlayMode)
            {
                ImGui::SameLine();
                if (ImGui::SmallButton("Edit") && !entry.Path.empty())
                    host.OpenScriptInEditor(entry.Path);
                ImGui::SameLine();
                if (ImGui::SmallButton("x"))
                    removeWsIdx = i;

                bool enabled = entry.Enabled;
                if (ImGui::Checkbox("Enabled", &enabled))
                    entry.Enabled = enabled;
            }

            ImGui::Separator();
            ImGui::PopID();
        }

        if (removeWsIdx >= 0)
            worldScripts.erase(worldScripts.begin() + removeWsIdx);

        if (worldScripts.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.57f, 1.0f));
            ImGui::TextUnformatted("No world scripts");
            ImGui::PopStyleColor();
        }

        if (!inPlayMode)
        {
            ImGui::Spacing();
            if (ImGui::Button("+ New World Script"))
                host.CreateAndAttachWorldScript();
            ImGui::SameLine();
            if (ImGui::Button("Browse..."))
            {
                std::string picked = CHEngine::FileDialog::OpenFile("Lua Script", "*.lua");
                if (!picked.empty())
                    worldScripts.push_back(CHEngine::ScriptEntry{ picked, true });
            }
        }
        ImGui::Spacing();
    }
    // ─────────────────────────────────────────────────────────────────────────

    // Кнопки добавления объектов убраны — используй Shift+A
    ImGui::Spacing();

    // Pass 1: collect all entities with display info
    struct EntityInfo {
        CHEngine::EntityHandle handle;
        CHEngine::UUID         uuid;
        std::string            name;
        const char*            icon;
    };
    std::vector<EntityInfo>                       allEntities;
    std::map<CHEngine::UUID, std::vector<size_t>> children;   // parentUUID → child indices
    std::vector<size_t>                           roots;

    scene_ptr->ForEach<CHEngine::TagComponent>(
        [&](CHEngine::EntityHandle handle, const CHEngine::UUID& id, CHEngine::TagComponent& tag)
    {
        const char* icon = "";
        if (const auto* ent = scene_ptr->TryGetEntity(handle);
            ent && ent->HasComponent<CHEngine::LightComponent>())
        {
            const auto type = ent->GetComponent<CHEngine::LightComponent>().LightData.Type;
            if      (type == CHEngine::LightType::Directional) icon = "[D] ";
            else if (type == CHEngine::LightType::Point)        icon = "[P] ";
            else if (type == CHEngine::LightType::Spot)         icon = "[S] ";
        }
        allEntities.push_back({ handle, id, tag.Name, icon });
    });

    // Pass 2: build parent→children map; orphaned/missing parents → root
    std::map<CHEngine::UUID, size_t> uuidToIndex;
    for (size_t i = 0; i < allEntities.size(); ++i)
        uuidToIndex[allEntities[i].uuid] = i;

    for (size_t i = 0; i < allEntities.size(); ++i)
    {
        const auto* ent = scene_ptr->TryGetEntity(allEntities[i].handle);
        CHEngine::UUID parentUUID;
        if (ent && ent->HasComponent<CHEngine::ParentNodeComponent>())
            parentUUID = ent->GetComponent<CHEngine::ParentNodeComponent>().Value;

        if (parentUUID.IsValid() && uuidToIndex.count(parentUUID))
            children[parentUUID].push_back(i);
        else
            roots.push_back(i);
    }

    std::optional<CHEngine::UUID> deleteID;

    std::function<void(size_t)> renderNode = [&](size_t idx)
    {
        const EntityInfo& info = allEntities[idx];
        bool isSelected  = (info.handle == activeSession->SelectedEntity);
        bool hasChildren = children.count(info.uuid) > 0;

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;
        flags |= hasChildren ? ImGuiTreeNodeFlags_OpenOnArrow : ImGuiTreeNodeFlags_Leaf;
        if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

        ImGui::PushID(static_cast<int>(std::hash<CHEngine::UUID>{}(info.uuid)));
        bool opened = ImGui::TreeNodeEx("##object", flags, "  %s%s", info.icon, info.name.c_str());

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            host.SetSelection(info.handle);

        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Focus (F)"))
            {
                host.GetCommandStack().Push(MakeScope<CallbackCommand>(
                    [&host] { host.FocusOnSelected(); }, [] {}, false));
            }

            if (const auto* ent = scene_ptr->TryGetEntity(info.handle);
                ent && (ent->HasComponent<CHEngine::UIOverlayCanvasComponent>() ||
                        ent->HasComponent<CHEngine::UIWorldCanvasComponent>()   ||
                        ent->HasComponent<CHEngine::UIPanelComponent>()))
            {
                ImGui::Separator();
                if (ImGui::BeginMenu("Add UI"))
                {
                    const auto& uuid = ent->GetComponent<CHEngine::IDComponent>().Value;
                    if (ImGui::MenuItem("Panel"))  { host.SetSelection(info.handle); host.AddUIPanel(uuid);  }
                    if (ImGui::MenuItem("Text"))   { host.SetSelection(info.handle); host.AddUIText(uuid);   }
                    if (ImGui::MenuItem("Button")) { host.SetSelection(info.handle); host.AddUIButton(uuid); }
                    if (ImGui::MenuItem("Image"))  { host.SetSelection(info.handle); host.AddUIImage(uuid);  }
                    if (ImGui::MenuItem("Slider")) { host.SetSelection(info.handle); host.AddUISlider(uuid); }
                    ImGui::EndMenu();
                }
            }

            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.231f, 0.188f, 1.0f));
            if (ImGui::MenuItem("Delete")) deleteID = info.uuid;
            ImGui::PopStyleColor();
            ImGui::EndPopup();
        }

        if (opened)
        {
            if (hasChildren)
                for (size_t childIdx : children[info.uuid])
                    renderNode(childIdx);
            ImGui::TreePop();
        }
        ImGui::PopID();
    };

    for (size_t rootIdx : roots)
        renderNode(rootIdx);

    if (allEntities.empty())
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
