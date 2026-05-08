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

    void BasicFrameGraphFrontend::Compile()
    {
        const int n = static_cast<int>(m_Passes.size());
        if (n == 0) { m_Sorted.clear(); return; }

        // Build adjacency: edges[i] = set of passes that must come after i
        std::vector<std::vector<int>> adj(n);
        std::vector<int>              inDeg(n, 0);

        // Process reads THEN writes per-pass so that read-modify-write passes
        // (e.g. GridPass: reads LDR from Tonemap, writes LDR back) are correctly
        // ordered. If we processed all writes first, GridPass would overwrite
        // lastWriter[LDR] before TonemapPass→GridPass edge is added.
        std::unordered_map<uint32_t, int> lastWriter; // texture index → last writer pass

        for (int i = 0; i < n; ++i)
        {
            // 1. Reads: add edges from previous writers to this pass
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
            // 2. Writes: update lastWriter AFTER reads so self-reads work correctly
            for (const TextureHandle& w : m_Passes[i].Writes)
                if (w.IsValid())
                    lastWriter[w.index] = i;
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
            m_Sorted.push_back(m_Passes[idx]);
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
