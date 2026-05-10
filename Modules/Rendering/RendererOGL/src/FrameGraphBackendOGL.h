#pragma once

#include "Render/IFrameGraphBackend.h"
#include "Render/Graph/PassDesc.h"
#include "Render/Handles.h"

#include <glad/glad.h>
#include <unordered_map>

namespace CHModules {

    struct RenderFactoryOGL;

    /// OpenGL implementation of IFrameGraphBackend.
    /// Owns VAO and FBO caches keyed by resource handles.
    class FrameGraphBackendOGL final : public CHEngine::IFrameGraphBackend
    {
    public:
        explicit FrameGraphBackendOGL(RenderFactoryOGL& factory);
        ~FrameGraphBackendOGL() override;

        void Execute(const CHEngine::Vector<CHEngine::PassDesc>& passes) override;

    private:
        // ── VAO cache ─────────────────────────────────────────────────────────
        // Key includes (index, generation) per resource so a recycled handle
        // index produces a fresh cache entry (avoids stale GL state on resize/recreate).
        struct VaoKey {
            uint32_t pipelineIdx, pipelineGen;
            uint32_t vbIdx,       vbGen;
            uint32_t ibIdx,       ibGen;
            bool operator==(const VaoKey& o) const {
                return pipelineIdx == o.pipelineIdx && pipelineGen == o.pipelineGen
                    && vbIdx == o.vbIdx && vbGen == o.vbGen
                    && ibIdx == o.ibIdx && ibGen == o.ibGen;
            }
        };
        struct VaoKeyHash {
            size_t operator()(const VaoKey& k) const {
                size_t h = std::hash<uint32_t>{}(k.pipelineIdx);
                auto mix = [&h](uint32_t v) {
                    h ^= std::hash<uint32_t>{}(v) + 0x9e3779b9u + (h << 6) + (h >> 2);
                };
                mix(k.pipelineGen); mix(k.vbIdx); mix(k.vbGen); mix(k.ibIdx); mix(k.ibGen);
                return h;
            }
        };
        std::unordered_map<VaoKey, GLuint, VaoKeyHash> m_VaoCache;

        // ── FBO cache ─────────────────────────────────────────────────────────
        // Key: color + depth attachment (index + generation each).
        struct FboKey {
            uint32_t colorIdx, colorGen;
            uint32_t depthIdx, depthGen;
            bool operator==(const FboKey& o) const {
                return colorIdx == o.colorIdx && colorGen == o.colorGen
                    && depthIdx == o.depthIdx && depthGen == o.depthGen;
            }
        };
        struct FboKeyHash {
            size_t operator()(const FboKey& k) const {
                size_t h = std::hash<uint32_t>{}(k.colorIdx);
                auto mix = [&h](uint32_t v) {
                    h ^= std::hash<uint32_t>{}(v) + 0x9e3779b9u + (h << 6) + (h >> 2);
                };
                mix(k.colorGen); mix(k.depthIdx); mix(k.depthGen);
                return h;
            }
        };
        std::unordered_map<FboKey, GLuint, FboKeyHash> m_FboCache;

        // ── Helpers ───────────────────────────────────────────────────────────
        GLuint GetOrCreateVAO(const CHEngine::PassDesc& pass, const CHEngine::DrawDesc& draw);
        GLuint GetOrCreateFBO(const CHEngine::PassDesc& pass);

        RenderFactoryOGL& m_Factory; // NOLINT — struct, not class
    };

} // namespace CHModules
