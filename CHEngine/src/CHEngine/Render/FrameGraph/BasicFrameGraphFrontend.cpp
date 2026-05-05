#include "chepch.h"
#include "BasicFrameGraphFrontend.h"

#include <Log/Log.h>
#include <unordered_map>
#include <queue>

namespace CHEngine {

    void BasicFrameGraphFrontend::Reset()
    {
        m_Passes.clear();
        m_Sorted.clear();
    }

    void BasicFrameGraphFrontend::AddPass(PassDesc&& pass)
    {
        m_Passes.push_back(std::move(pass));
    }

    // ─── Kahn's algorithm topological sort ───────────────────────────────────
    // Edges: if pass A writes a texture that pass B reads, B depends on A.
    // For MVP (2-3 passes added in insertion order), this degenerates to
    // the original order — but the sort handles arbitrary permutations correctly.

    void BasicFrameGraphFrontend::Compile()
    {
        const int n = static_cast<int>(m_Passes.size());
        if (n == 0) { m_Sorted.clear(); return; }

        // Build adjacency: edges[i] = set of passes that must come after i
        std::vector<std::vector<int>> adj(n);
        std::vector<int>              inDeg(n, 0);

        // Map TextureHandle → index of the last pass that writes it
        // (only simple single-writer model needed for MVP)
        std::unordered_map<uint32_t, int> lastWriter; // key = handle index

        for (int i = 0; i < n; ++i)
        {
            for (const TextureHandle& w : m_Passes[i].Writes)
                if (w.IsValid())
                    lastWriter[w.index] = i;
        }

        for (int i = 0; i < n; ++i)
        {
            for (const TextureHandle& r : m_Passes[i].Reads)
            {
                if (!r.IsValid()) continue;
                auto it = lastWriter.find(r.index);
                if (it == lastWriter.end()) continue;
                int writer = it->second;
                if (writer == i) continue; // self-loop, skip
                adj[writer].push_back(i);
                ++inDeg[i];
            }
        }

        // Kahn's BFS
        std::queue<int> ready;
        for (int i = 0; i < n; ++i)
            if (inDeg[i] == 0) ready.push(i);

        m_Sorted.clear();
        m_Sorted.reserve(static_cast<size_t>(n));

        while (!ready.empty())
        {
            int idx = ready.front();
            ready.pop();
            m_Sorted.push_back(m_Passes[idx]); // copy into sorted list
            for (int next : adj[idx])
            {
                if (--inDeg[next] == 0)
                    ready.push(next);
            }
        }

        if (static_cast<int>(m_Sorted.size()) != n)
            CHE_CORE_WARN("BasicFrameGraphFrontend::Compile: cycle detected — using partial order");
    }

    void BasicFrameGraphFrontend::Execute(IFrameGraphBackend& backend)
    {
        if (m_Sorted.empty())
        {
            CHE_CORE_WARN("BasicFrameGraphFrontend::Execute: no compiled passes — call Compile() first");
            return;
        }
        backend.Execute(m_Sorted);
    }

} // namespace CHEngine
