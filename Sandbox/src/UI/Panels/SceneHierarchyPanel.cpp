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

    // PNG хранит ширину и высоту в IHDR-чанке по фиксированным смещениям:
    // байты 16-19 = width (big-endian), байты 20-23 = height (big-endian)
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

// All scene hierarchy content — usable standalone or inside a tab.
void SceneHierarchyPanel::DrawContent(EditorUiHost& host)
{
    EnsureLogo();
    SceneSession& activeSession = host.GetActiveSceneSession();
    auto scene_ptr = activeSession.EditorScene;
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
        bool isSelected = (handle == activeSession.SelectedEntity);
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
                host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
                    [&host] { host.FocusOnSelected(); }, [] {}, false));
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
        // ── Подсказка сверху, логотип снизу ──────────────────────────────────
        const float panelW = ImGui::GetContentRegionAvail().x;

        // 1. Подсказка — по центру сверху
        ImGui::Spacing();
        const char* hint = "Press Shift+A to add objects";
        float textW = ImGui::CalcTextSize(hint).x;
        ImGui::SetCursorPosX((panelW - textW) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.52f, 1.0f));
        ImGui::TextUnformatted(hint);
        ImGui::PopStyleColor();

        // 2. Пустое пространство
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();

        // 3. Логотип — центрируем, флипаем Y (ImGui/OGL хранит текстуры перевёрнутыми)
        if (m_Logo.IsValid())
        {
            auto* f = CHEngine::RenderFacade::GetRenderFactory();
            ImTextureID texId = f ? static_cast<ImTextureID>(f->GetTextureNativeID(m_Logo))
                                  : static_cast<ImTextureID>(0);
            if (texId)
            {
                const float logoW = panelW - 20.0f;
                const float logoH = (m_LogoAspect > 0.0f) ? logoW / m_LogoAspect : logoW;

                // Флип по Y: uv0=(0,1), uv1=(1,0) — картинка была перевёрнута
                const ImVec2 uv0(0.0f, 1.0f);
                const ImVec2 uv1(1.0f, 0.0f);

                ImGui::SetCursorPosX((panelW - logoW) * 0.5f);
                ImVec2 p = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddImage(
                    texId, p, ImVec2(p.x + logoW, p.y + logoH),
                    uv0, uv1, IM_COL32(255, 255, 255, 90)); // ~35% opacity
                ImGui::Dummy(ImVec2(logoW, logoH));
            }
        }
    }

    if (ImGui::BeginPopupContextWindow("scene_hierarchy_ctx",
            ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
    {
        if (activeSession.SessionState == SceneSession::State::Edit)
            if (ImGui::MenuItem("Create empty entity")) host.AddEmptyEntity();
        ImGui::EndPopup();
    }

    if (deleteID.has_value())
        host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
            [&host, id = *deleteID] { host.DestroyEntityByUuid(id); }, [] {}, false));
}

void SceneHierarchyPanel::Draw(EditorUiHost& host, ImVec2 pos, ImVec2 size, bool reset_layout)
{
    UIActive::BeginPanel("Scene", pos, size, 0, reset_layout);
    DrawContent(host);
    UIActive::EndPanel();
}

} // namespace Sandbox
