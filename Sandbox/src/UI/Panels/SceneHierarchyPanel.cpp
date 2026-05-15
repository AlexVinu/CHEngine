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

        // ── ASCII арт — посимвольный рендеринг в строгой моноширинной сетке ────
        //
        // Каждый символ рисуется вручную через DrawList::AddText с фиксированным
        // шагом cellW по X и cellH по Y — гарантированная равномерная сетка.
        //
        // Арт занимает ~90 колонок. cellW = avail / 90.
        // cellH = cellW * 1.7  (типичное соотношение для моноширинных шрифтов).

        const float kCols  = 90.0f;
        const float cellW  = avail / kCols;
        const float cellH  = cellW * 1.7f;

        // Берём моноширинный шрифт если есть, иначе дефолтный
        ImFont* font = UIThemeRetro::g_FontMono ? UIThemeRetro::g_FontMono : ImGui::GetFont();
        const ImU32 artColor = IM_COL32(90, 115, 180, 178);

        // Резервируем место в layout сразу — никаких frame-lag артефактов
        const float totalH = cellH * kAsciiArtLines;
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(avail, totalH));

        // Рисуем поверх зарезервированного места
        ImDrawList* dl = ImGui::GetWindowDrawList();
        char buf[2] = { 0, 0 };

        for (int row = 0; row < kAsciiArtLines; ++row)
        {
            const char* line = kAsciiArt[row];
            const float y    = origin.y + row * cellH;

            for (int col = 0; line[col] != '\0'; ++col)
            {
                if (line[col] == ' ') continue; // пробелы пропускаем
                buf[0] = line[col];
                dl->AddText(font, cellH, ImVec2(origin.x + col * cellW, y),
                            artColor, buf, buf + 1);
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
