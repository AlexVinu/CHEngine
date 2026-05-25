#pragma once
#include "CHEngine/Scene/Scene.h"
#include "CHEngine/Scene/Components.h"
#include "CHEngine/World/ISystem.h"
#include "Render/UniformBlocks.h"
#include "Render/Handles.h"
#include "Render/Descriptors.h"
#include "CHEngine/Camera/EditorCamera.h"
#include "CHEngine/Render/DrawItem.h"

#include <vector>
#include <functional>

namespace CHEngine {

    class World;
    class DeferredOps;
    class Timestep;

    class RenderSystem : public ISystem
    {
    public:
        RenderSystem()
            : ISystem(SystemPhase::Presentation, 10)
        {
        }

        const char* GetName() const override { return "RenderSystem"; }
        void Run(World& world, DeferredOps& deferred_ops, Timestep ts) override;

        // Re-read TransformComponent for every draw item and re-upload the object UBO
        // ring buffer. Call this after any late transform update (e.g. gizmo drag) but
        // before RenderFacade::EndFrame() so the GPU sees the latest transforms.
        void RefreshTransforms(Scene& scene);

    private:
        // Build cameraUBO, returns false when no camera is available.
        bool ResolveCamera(World& world, Scene& scene, UBOCamera& out);

        // Collect all light sources and populate lightingUBO.
        void CollectLighting(Scene& scene, UBOLighting& out);

        // Select primary CameraComponent from ECS.
        const CameraComponent* SelectActiveCamera(Scene& scene, EntityHandle& outHandle);

        // Extract visible mesh entities into a flat draw list.
        void BuildDrawList(Scene& scene);

        // Lazy-initialize persistent GPU resources for MainColorPass.
        bool EnsureGPUResources();

        // Ensure object UBO ring buffer is large enough for the current draw list.
        bool EnsureObjectUBO(uint32_t requiredDraws);

    private:
        std::vector<DrawItem> m_DrawList;

    // Pipeline cache: each unique shader gets its own pipeline.
    PipelineHandle m_MeshPipeline;
    ShaderHandle   m_PBRShader{};
    PipelineHandle m_PBRPipeline{};

    // Get or create a pipeline for a given shader handle.
    PipelineHandle GetOrCreatePipeline(ShaderHandle shader);
        ShaderHandle   m_TonemapShader;
        PipelineHandle m_TonemapPipeline;
        BufferHandle   m_TonemapParamsUBO;  // {GammaCorrect, pad, pad, pad}
        BufferHandle   m_CameraUBO;
        BufferHandle   m_LightingUBO;
        BufferHandle   m_DefaultMaterialUBO; // default material: UseTexture=0

        // Render targets
        TextureHandle m_HDRTarget;
        TextureHandle m_DepthTarget;
        TextureHandle m_LDRTarget;

        // Per-draw Object UBO ring buffer.
        BufferHandle m_ObjectUBORing;
        uint32_t     m_ObjectUBOAlignedStride = 0;
        uint32_t     m_ObjectUBOCapacity      = 0;

        // Per-draw Material UBO ring buffer.
        BufferHandle m_MaterialUBORing;
        uint32_t     m_MaterialUBOAlignedStride = 0;
        uint32_t     m_MaterialUBOCapacity      = 0;
        bool EnsureMaterialUBO(uint32_t requiredDraws);

        uint32_t m_LastViewportW = 0;
        uint32_t m_LastViewportH = 0;
    };

} // namespace CHEngine
