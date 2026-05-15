#include "SceneHierarchyPanel.h"

#include "SceneSession.h"
#include "SetTransformCommand.h"

#include <CHEngine/Utils/FileDialog.h>
#include <CHEngine/Scene/Components.h>

#include "UIThemeActive.h"

#include <boost/container_hash/hash.hpp>
#include <optional>

namespace Sandbox {

// ASCII-арт для пустой сцены (встроен в код, не читается с диска)
static const char* const kAsciiArt[] = {
    "                                                          %%%%%%%%%%%%%%%                    ",
    "                                                     %%%%%%%%%%%%%%%%%%%%%%%%%               ",
    "                                                  %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%            ",
    "                                                %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%          ",
    "                                               %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%         ",
    "                                              %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%@       ",
    "                                             %%%%%%%%%%%%%%%          %%%%%%%%%%%%%%%%       ",
    "                                    %%      %%%%%%%%%%%%%%              %%%%%%%%%%%%%%%      ",
    "                             %%%%%%%%%      %%%%%%%%%%%%%                %%%%%%%%%%%%%%      ",
    "                         %%%%%%%%%%%%%      %%%%%%%%%%%%                 %%%%%%%%%%%%%%      ",
    "                     %%%%%%%%%%%%%%%%%%                                  %%%%%%%%%%%%%%      ",
    "                  %%%%%%%%%%%%%%%%%%%%%                                  %%%%%%%%%%%%%%      ",
    "                %%%%%%%%%%%%%%%%                                        %%%%%%%%%%%%%%       ",
    "              %%%%%%%%%%%%%%                                          %%%%%%%%%%%%%%%%       ",
    "           %%%%%%%%%%%%%                                           %%%%%%%%%%%%%%%%%%        ",
    "          %%%%%%%%%%%                                           %%%%%%%%%%%%%%%%%%%          ",
    "        %%%%%%%%%%%                                           %%%%%%%%%%%%%%%%%%%            ",
    "      %%%%%%%%%%%                                           %%%%%%%%%%%%%%%%%%%              ",
    "     %%%%%%%%%%                                            %%%%%%%%%%%%%%%%                  ",
    "    %%%%%%%%%                                             %%%%%%%%%%%%%%%                    ",
    "   %%%%%%%%%                                              %%%%%%%%%%%%%              %%%%    ",
    "  %%%%%%%%%                                              %%%%%%%%%%%%%             %%%%%%%   ",
    " %%%%%%%%%                                               %%%%%%%%%%%%%            %%%%%%%%%  ",
    "%%%%%%%%%                                                 %%%%%%%%%%%%              %%%%%%%% ",
    "%%%%%%%%                                                                             %%%%%%%%",
    "%%%%%%%%                                                                              %%%%%%%",
    " %%%%%%%%                                                                                %%%%",
    " %%%%%%%%                 %%%%%%%                               %%%%%%                   %%%%",
    " %%%%%%%                %%%%%%%%%%%                          %%%%%%%%%%%%                %%%%",
    "%%%%%%%%               %%%%%%%%%%%%%%                       %%%%%%%%%%%%%%               %%%%",
    "%%%%%%%%               %%%%%%%%%%%%%%%                       %%%%%%%%%%%%%%              %%%%",
    "%%%%%%%%               %%%%%%%%%%%%%%%                      %%%%%%%%%%%%%%%              %%%%",
    "%%%%%%%%                %%%%%%%%%%%%%%                       %%%%%%%%%%%%%%              %%%%",
    "%%%%%%%%                 %%%%%%%%%%%%                         %%%%%%%%%%%%               %%%%",
    "%%%%%%%%                   @%%%%%%                               %%%%%%                  %%%%",
    "%%%%%%%%                                                                                 %%%%",
    "%%%%%%%%                                                                                 %%%%",
    " %%%%%%%                                                                                %%%% ",
    " %%%%%%%%                                                                               %%%% ",
    " %%%%%%%%                                                                              %%%%% ",
    "  %%%%%%%%                                                                             %%%%  ",
    "  %%%%%%%%%                    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%                   %%%%   ",
    "   %%%%%%%%                   %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%                  %%%%   ",
    "    %%%%%%%%                 %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%                 %%%%    ",
    "     %%%%%%%%               %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%               %%%%%     ",
    "      %%%%%%%%@              %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%              %%%%%      ",
    "       %%%%%%%%%              %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%              %%%%%       ",
    "        %%%%%%%%%              %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%              %%%%%        ",
    "         %%%%%%%%%%                                                           %%%%%%%%%%     ",
    "          %%%%%%%%%%%                                                       %%%%%%%%%%%      ",
    "            %%%%%%%%%%%                                                   %%%%%%%%%%%        ",
    "             %%%%%%%%%%%%                                               %%%%%%%%%%%          ",
    "               %%%%%%%%%%%%%                                         %%%%%%%%%%%%%          ",
    "                 %%%%%%%%%%%%%%                                   %%%%%%%%%%%%%%             ",
    "                   %%%%%%%%%%%%%%%%                           %%%%%%%%%%%%%%%%               ",
    "                      %%%%%%%%%%%%%%%%%%%%             %%%%%%%%%%%%%%%%%%%%                  ",
    "                         %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%                  ",
    "                             %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%                      ",
    "                                 %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%                          ",
    "                                       %%%%%%%%%%%%%%%%%%%%%                                 ",
};
static constexpr int kAsciiArtLines = static_cast<int>(sizeof(kAsciiArt) / sizeof(kAsciiArt[0]));

// All scene hierarchy content — usable standalone or inside a tab.
void SceneHierarchyPanel::DrawContent(EditorUiHost& host)
{
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
        const float avail = ImGui::GetContentRegionAvail().x;
        if (avail < 32.0f) return; // первый кадр — пропускаем

        // ── Подсказка ─────────────────────────────────────────────────────────
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.52f, 1.0f));
        ImGui::TextUnformatted("Press Shift+A to add objects");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        // ── ASCII арт ─────────────────────────────────────────────────────────
        // Вычисляем размер символа чтобы арт вписался в ширину панели.
        // Арт занимает ~90 символов в ширину; масштаб ≈ avail / (90 * charW).
        // Используем моноширинный шрифт (g_FontMono) через SetWindowFontScale.
        const float kArtCols = 90.0f;
        // Примерная ширина символа = 0.55 * fontHeight при дефолтном масштабе
        const float defaultFontH = ImGui::GetFontSize();
        const float defaultCharW = defaultFontH * 0.55f;
        float scale = avail / (kArtCols * defaultCharW);
        scale = ImClamp(scale, 0.20f, 0.55f); // не меньше 20% и не больше 55%

        const float lineH = defaultFontH * scale;

        ImGui::SetWindowFontScale(scale);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.45f, 0.70f, 0.7f));

        for (int i = 0; i < kAsciiArtLines; ++i)
            ImGui::TextUnformatted(kAsciiArt[i]);

        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        ImGui::SetWindowFontScale(1.0f); // возвращаем масштаб
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
