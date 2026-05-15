#include "ProfilerPanel.h"

#include <Profiler.h>

#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <vector>

#include <algorithm>
#include <cstring>
#include <vector>

namespace Sandbox {

void ProfilerPanel::Draw(SceneViewLayerHost& host)
{
    if (!host.GetShowProfiler())
        return;

    ImGui::SetNextWindowSize(ImVec2(560, 380), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Profiler", &host.GetShowProfiler()))
    {
        ImGui::End();
        return;
    }

    // FPS counter (moved here from toolbar)
    float fps = ImGui::GetIO().Framerate;
    float ms  = 1000.0f / (fps > 0.0f ? fps : 1.0f);
    ImGui::Text("%.0f fps  (%.2f ms)", fps, ms);
    ImGui::SameLine(0, 16);

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

void ProfilerPanel::DrawInPanel(SceneViewLayerHost& /*host*/)
{
    // Tiling mode: window pos/size set externally, open unconditionally
    if (!ImGui::Begin("Profiler"))
    {
        ImGui::End();
        return;
    }

    float fps = ImGui::GetIO().Framerate;
    float ms  = 1000.0f / (fps > 0.0f ? fps : 1.0f);
    ImGui::Text("%.0f fps  (%.2f ms)", fps, ms);
    ImGui::SameLine(0, 16);
    if (ImGui::Button("Reset"))
        CHEngine::Profiler::Reset();
    ImGui::SameLine();
    ImGui::TextDisabled("(F3 to toggle)");

    const auto& stats = CHEngine::Profiler::GetFrameStats();

    if (ImGui::BeginTable("##profiler_tbl", 5,
        ImGuiTableFlags_Sortable | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Name",     ImGuiTableColumnFlags_DefaultSort,          0.f, 0);
        ImGui::TableSetupColumn("Calls",    ImGuiTableColumnFlags_PreferSortDescending, 0.f, 1);
        ImGui::TableSetupColumn("Total ms", ImGuiTableColumnFlags_PreferSortDescending, 0.f, 2);
        ImGui::TableSetupColumn("Avg ms",   ImGuiTableColumnFlags_PreferSortDescending, 0.f, 3);
        ImGui::TableSetupColumn("Max ms",   ImGuiTableColumnFlags_PreferSortDescending, 0.f, 4);
        ImGui::TableHeadersRow();

        struct E { const char* name; int calls; double tot, avg, max; };
        std::vector<E> entries;
        entries.reserve(stats.size());
        for (auto& [n, s] : stats)
        {
            double a = s.calls > 0 ? s.totalMs / s.calls : 0.0;
            entries.push_back({ n, s.calls, s.totalMs, a, s.maxMs });
        }
        if (auto* sp = ImGui::TableGetSortSpecs(); sp && sp->SpecsDirty && !entries.empty())
        {
            int col = sp->Specs[0].ColumnIndex;
            bool asc = sp->Specs[0].SortDirection == ImGuiSortDirection_Ascending;
            std::sort(entries.begin(), entries.end(), [col, asc](const E& a, const E& b){
                bool lt = false;
                switch (col) {
                case 0: lt = strcmp(a.name,b.name)<0; break;
                case 1: lt = a.calls<b.calls; break;
                case 2: lt = a.tot<b.tot; break;
                case 3: lt = a.avg<b.avg; break;
                case 4: lt = a.max<b.max; break;
                }
                return asc ? lt : !lt;
            });
            sp->SpecsDirty = false;
        }
        for (auto& e : entries)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(e.name);
            ImGui::TableSetColumnIndex(1); ImGui::Text("%d",    e.calls);
            ImGui::TableSetColumnIndex(2); ImGui::Text("%.3f",  e.tot);
            ImGui::TableSetColumnIndex(3); ImGui::Text("%.3f",  e.avg);
            ImGui::TableSetColumnIndex(4); ImGui::Text("%.3f",  e.max);
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

} // namespace Sandbox
