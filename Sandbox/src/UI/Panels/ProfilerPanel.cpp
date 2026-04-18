#include "ProfilerPanel.h"

#include <Profiler.h>

#include <imgui.h>

#include <algorithm>
#include <cstring>
#include <vector>

namespace Sandbox {

void ProfilerPanel::Draw(EditorUiHost& host)
{
    if (!host.GetShowProfiler())
        return;

    ImGui::SetNextWindowSize(ImVec2(560, 380), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Profiler", &host.GetShowProfiler()))
    {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Reset"))
        CHEngine::Profiler::Reset();
    ImGui::SameLine();
    ImGui::TextDisabled("(F3 to toggle)");

    const auto& stats = CHEngine::Profiler::GetFrameStats();

    if (ImGui::BeginTable("##profiler_table", 5,
 ImGuiTableFlags_Sortable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_DefaultSort, 0.0f, 0);
        ImGui::TableSetupColumn("Calls", ImGuiTableColumnFlags_PreferSortDescending, 0.0f, 1);
        ImGui::TableSetupColumn("Total ms", ImGuiTableColumnFlags_PreferSortDescending, 0.0f, 2);
        ImGui::TableSetupColumn("Avg ms", ImGuiTableColumnFlags_PreferSortDescending, 0.0f, 3);
        ImGui::TableSetupColumn("Max ms", ImGuiTableColumnFlags_PreferSortDescending, 0.0f, 4);
        ImGui::TableHeadersRow();

        struct Entry {
            const char* name;
            int calls;
            double totalMs;
            double avgMs;
            double maxMs;
        };
        std::vector<Entry> entries;
        entries.reserve(stats.size());
        for (auto& [name, stat] : stats)
        {
            double avg = stat.calls > 0 ? stat.totalMs / stat.calls : 0.0;
            entries.push_back({ name, stat.calls, stat.totalMs, avg, stat.maxMs });
        }

        if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs())
        {
            if (specs->SpecsDirty && specs->SpecsCount > 0)
            {
                const auto& spec = specs->Specs[0];
                std::sort(entries.begin(), entries.end(), [&](const Entry& a, const Entry& b) {
                    int cmp = 0;
                    switch (spec.ColumnUserID)
                    {
                    case 0:
                        cmp = std::strcmp(a.name, b.name);
                        break;
                    case 1:
                        cmp = (a.calls < b.calls) ? -1 : (a.calls > b.calls) ? 1 : 0;
                        break;
                    case 2:
                        cmp = (a.totalMs < b.totalMs) ? -1 : (a.totalMs > b.totalMs) ? 1 : 0;
                        break;
                    case 3:
                        cmp = (a.avgMs < b.avgMs) ? -1 : (a.avgMs > b.avgMs) ? 1 : 0;
                        break;
                    case 4:
                        cmp = (a.maxMs < b.maxMs) ? -1 : (a.maxMs > b.maxMs) ? 1 : 0;
                        break;
                    }
                    return (spec.SortDirection == ImGuiSortDirection_Ascending) ? cmp < 0 : cmp > 0;
                });
                specs->SpecsDirty = false;
            }
        }

        for (auto& e : entries)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(e.name);
            ImGui::TableNextColumn();
            ImGui::Text("%d", e.calls);
            ImGui::TableNextColumn();
            ImGui::Text("%.3f", e.totalMs);
            ImGui::TableNextColumn();
            ImGui::Text("%.3f", e.avgMs);
            ImGui::TableNextColumn();
            ImGui::Text("%.3f", e.maxMs);
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

} // namespace Sandbox
