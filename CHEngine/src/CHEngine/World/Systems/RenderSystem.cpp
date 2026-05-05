#include "chepch.h"
#include "RenderSystem.h"
#include "CHEngine/Utils/MathUtils.h"

#include "CHEngine/Mesh/Material.h"
#include "CHEngine/Mesh/Mesh.h"
#include "CHEngine/Render/RenderFacade.h"
#include "CHEngine/Scene/Entity.h"
#include "CHEngine/World/World.h"
#include "CHEngine/Application.h"
#include "Log/Log.h"
#include <Render/UniformBlocks.h>
#include <Render/IRenderFactory.h>
#include <Render/Graph/PassDesc.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/matrix.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace CHEngine {

    namespace {

        constexpr float kDefaultAmbientR = 0.15f;
        constexpr float kDefaultAmbientG = 0.15f;
        constexpr float kDefaultAmbientB = 0.20f;

        constexpr glm::vec3 kDefaultLightDir      = { -0.3f, -1.0f, -0.5f };
        constexpr glm::vec3 kDefaultLightColor    = {  1.0f,  1.0f,  0.95f };
        constexpr float     kDefaultLightIntensity = 1.0f;

    } // namespace

    // ============================================================================
    //  Public entry point
    // ============================================================================

    bool RenderSystem::EnsureGPUResources()
    {
        IRenderFactory* factory = RenderFacade::GetRenderFactory();
        if (!factory) return false;

        const uint32_t vw = RenderFacade::GetViewportWidth();
        const uint32_t vh = RenderFacade::GetViewportHeight();
        if (vw == 0 || vh == 0) return false;

        const bool sizeChanged = (vw != m_LastViewportW || vh != m_LastViewportH);

        // Recreate render targets if viewport size changed.
        if (sizeChanged || !m_HDRTarget.IsValid())
        {
            if (m_HDRTarget.IsValid())   { factory->Delete(m_HDRTarget);   m_HDRTarget   = {}; }
            if (m_DepthTarget.IsValid()) { factory->Delete(m_DepthTarget); m_DepthTarget = {}; }
            if (m_LDRTarget.IsValid())   { factory->Delete(m_LDRTarget);   m_LDRTarget   = {}; }

            m_HDRTarget = factory->CreateTexture(
                nullptr, vw, vh, 4,
                1, 1,
                TextureFormat::RGBA16_FLOAT,
                TextureType::Texture2D,
                static_cast<TextureUsage>(
                    static_cast<uint32_t>(TextureUsage::RenderTarget) |
                    static_cast<uint32_t>(TextureUsage::ShaderResource)),
                MemoryType::GpuOnly,
                "HDRTarget");

            m_DepthTarget = factory->CreateTexture(
                nullptr, vw, vh, 0,
                1, 1,
                TextureFormat::D24_UNORM_S8_UINT,
                TextureType::Texture2D,
                TextureUsage::DepthStencil,
                MemoryType::GpuOnly,
                "DepthTarget");

            m_LDRTarget = factory->CreateTexture(
                nullptr, vw, vh, 4,
                1, 1,
                TextureFormat::RGBA8_UNORM,
                TextureType::Texture2D,
                static_cast<TextureUsage>(
                    static_cast<uint32_t>(TextureUsage::RenderTarget) |
                    static_cast<uint32_t>(TextureUsage::ShaderResource)),
                MemoryType::GpuOnly,
                "LDRTarget");

            m_LastViewportW = vw;
            m_LastViewportH = vh;

            if (!m_HDRTarget.IsValid() || !m_DepthTarget.IsValid() || !m_LDRTarget.IsValid())
            {
                CHE_CORE_ERROR("RenderSystem: failed to create render targets ({}x{})", vw, vh);
                return false;
            }

            RenderFacade::SetViewportOutputTexture(m_LDRTarget);
        }

        // Create MeshPipeline once (or when default shader becomes available).
        if (!m_MeshPipeline.IsValid())
        {
            ShaderHandle defaultShader = RenderFacade::GetDefaultMeshShader();
            if (!defaultShader.IsValid()) return false;

            PipelineDesc pipeDesc;
            pipeDesc.Shader       = defaultShader;
            pipeDesc.VertexLayout = GetStandardMeshLayout();
            pipeDesc.Primitive    = PrimitiveType::Triangles;
            pipeDesc.Depth.Test   = true;
            pipeDesc.Depth.Write  = true;
            pipeDesc.Depth.Compare = CompareOp::Less;
            pipeDesc.Raster.Cull  = CullMode::Back;
            pipeDesc.Raster.Face  = FrontFace::CCW;
            pipeDesc.ColorFormats.push_back(TextureFormat::RGBA16_FLOAT);
            pipeDesc.DepthFormat  = TextureFormat::D24_UNORM_S8_UINT;

            m_MeshPipeline = factory->CreatePipeline(std::move(pipeDesc));
            if (!m_MeshPipeline.IsValid())
            {
                CHE_CORE_ERROR("RenderSystem: failed to create MeshPipeline");
                return false;
            }
        }

        // Create Camera and Lighting UBOs once.
        if (!m_CameraUBO.IsValid())
        {
            m_CameraUBO = factory->CreateBuffer(
                sizeof(UBOCamera),
                BufferUsage::Uniform,
                MemoryType::CpuToGpu,
                {},
                String("CameraUBO"));
        }
        if (!m_LightingUBO.IsValid())
        {
            m_LightingUBO = factory->CreateBuffer(
                sizeof(UBOLighting),
                BufferUsage::Uniform,
                MemoryType::CpuToGpu,
                {},
                String("LightingUBO"));
        }

        // Load tonemap shader once.
        if (!m_TonemapShader.IsValid())
        {
            m_TonemapShader = RenderFacade::CreateShaderFromFile(
                String("Tonemap"), String("shaders/tonemap.slang"),
                String("vertMain"), String("fragMain"));
            if (!m_TonemapShader.IsValid())
                CHE_CORE_ERROR("RenderSystem: failed to load tonemap shader");
        }

        // Create tonemap pipeline once (depends on tonemap shader).
        if (!m_TonemapPipeline.IsValid() && m_TonemapShader.IsValid())
        {
            PipelineDesc tonemapDesc;
            tonemapDesc.Shader = m_TonemapShader;
            tonemapDesc.VertexLayout = VertexInputLayout(
                { VertexAttributeDesc(VertexFormat::Float2, 0, 0) }, 8u);
            tonemapDesc.Primitive      = PrimitiveType::Triangles;
            tonemapDesc.Depth.Test     = false;
            tonemapDesc.Depth.Write    = false;
            tonemapDesc.Raster.Cull    = CullMode::None;
            tonemapDesc.ColorFormats.push_back(TextureFormat::RGBA8_UNORM);

            m_TonemapPipeline = factory->CreatePipeline(std::move(tonemapDesc));
            if (!m_TonemapPipeline.IsValid())
                CHE_CORE_ERROR("RenderSystem: failed to create TonemapPipeline");
        }

        return m_HDRTarget.IsValid() && m_DepthTarget.IsValid() && m_LDRTarget.IsValid()
            && m_MeshPipeline.IsValid()
            && m_TonemapShader.IsValid() && m_TonemapPipeline.IsValid()
            && m_CameraUBO.IsValid() && m_LightingUBO.IsValid();
    }

    void RenderSystem::Run(World& world, DeferredOps& /*deferred_ops*/, Timestep /*ts*/)
    {
        static bool s_LoggedOnce = false;
        static int  s_FailureLogBudget = 5;

        auto scene = world.GetSceneRef();

        UBOCamera cameraUBO{};
        if (!ResolveCamera(world, *scene, cameraUBO))
        {
            if (s_FailureLogBudget-- > 0)
                CHE_CORE_WARN("RenderSystem: ResolveCamera failed, skipping frame");
            return;
        }

        UBOLighting lightingUBO{};
        CollectLighting(*scene, lightingUBO);

        BuildDrawList(*scene);

        // Phase 6 MVP: build and submit MainColorPass.
        if (!EnsureGPUResources())
        {
            if (s_FailureLogBudget-- > 0)
                CHE_CORE_WARN("RenderSystem: EnsureGPUResources failed (vw={}, vh={}, meshPipeline={}, tonemapPipeline={}, hdr={}, ldr={}, depth={}, defaultShader={})",
                              RenderFacade::GetViewportWidth(),
                              RenderFacade::GetViewportHeight(),
                              m_MeshPipeline.IsValid(),
                              m_TonemapPipeline.IsValid(),
                              m_HDRTarget.IsValid(),
                              m_LDRTarget.IsValid(),
                              m_DepthTarget.IsValid(),
                              RenderFacade::GetDefaultMeshShader().IsValid());
            return;
        }

        IRenderFactory* factory = RenderFacade::GetRenderFactory();
        if (!factory) return;

        // Upload per-frame UBOs.
        factory->UpdateBuffer(m_CameraUBO,
            std::span<const std::byte>(
                reinterpret_cast<const std::byte*>(&cameraUBO), sizeof(cameraUBO)));
        factory->UpdateBuffer(m_LightingUBO,
            std::span<const std::byte>(
                reinterpret_cast<const std::byte*>(&lightingUBO), sizeof(lightingUBO)));

        // Per-draw Object UBO ring buffer — ensure capacity, then upload all transforms in one call.
        const uint32_t drawCount = static_cast<uint32_t>(m_DrawList.size());
        if (drawCount > 0)
        {
            if (!EnsureObjectUBO(drawCount))
                return;

            std::vector<std::byte> ringStaging(
                static_cast<size_t>(drawCount) * m_ObjectUBOAlignedStride, std::byte{0});
            for (uint32_t i = 0; i < drawCount; ++i)
            {
                std::memcpy(ringStaging.data() + static_cast<size_t>(i) * m_ObjectUBOAlignedStride,
                            &m_DrawList[i].object,
                            sizeof(UBOObject));
            }
            factory->UpdateBuffer(m_ObjectUBORing,
                std::span<const std::byte>(ringStaging.data(), ringStaging.size()),
                0);
        }

        // Build MainColorPass.
        PassDesc mainColor;
        mainColor.Name            = "MainColorPass";
        mainColor.Pipeline        = m_MeshPipeline;
        mainColor.ColorAttachments.push_back(m_HDRTarget);
        mainColor.DepthAttachment = m_DepthTarget;
        mainColor.ColorLoadOp     = ELoadOp::Clear;
        mainColor.ColorStoreOp    = EStoreOp::Store;
        mainColor.ClearColor      = { 0.18f, 0.18f, 0.20f, 1.0f };
        mainColor.DepthLoadOp     = ELoadOp::Clear;
        mainColor.ClearDepth      = 1.0f;
        mainColor.ViewportWidth   = RenderFacade::GetViewportWidth();
        mainColor.ViewportHeight  = RenderFacade::GetViewportHeight();

        // Per-pass UBOs (Camera = slot 0, Lighting = slot 2).
        mainColor.Uniforms.push_back({ m_CameraUBO,  static_cast<uint32_t>(EUniformBlock::Camera),  0, 0 });
        mainColor.Uniforms.push_back({ m_LightingUBO, static_cast<uint32_t>(EUniformBlock::Lighting), 0, 0 });

        // Writes the HDR target (for topo-sort dependency tracking).
        mainColor.Writes.push_back(m_HDRTarget);

        // Build draw list — each draw binds its slice of the Object UBO ring buffer.
        for (uint32_t i = 0; i < drawCount; ++i)
        {
            const DrawItem& item = m_DrawList[i];

            DrawDesc draw;
            draw.VertexBuffer  = item.vertexBuffer;
            draw.IndexBuffer   = item.indexBuffer;
            draw.IdxFormat     = item.indexFormat;
            draw.IndexCount    = item.indexCount;
            draw.FirstIndex    = 0;
            draw.BaseVertex    = 0;
            draw.InstanceCount = 1;
            draw.Uniforms.push_back({
                m_ObjectUBORing,
                static_cast<uint32_t>(EUniformBlock::Object),
                static_cast<uint64_t>(i) * m_ObjectUBOAlignedStride,
                sizeof(UBOObject)
            });
            mainColor.Draws.push_back(std::move(draw));
        }

        if (!s_LoggedOnce)
        {
            CHE_CORE_INFO("RenderSystem: first MainColorPass dispatched (drawCount={}, viewport={}x{}, hdrTex={}, alignedStride={}, ringCapacity={})",
                          drawCount,
                          mainColor.ViewportWidth, mainColor.ViewportHeight,
                          m_HDRTarget.index,
                          m_ObjectUBOAlignedStride,
                          m_ObjectUBOCapacity);
            s_LoggedOnce = true;
        }

        RenderFacade::GetFrameGraph().AddPass(std::move(mainColor));

        // TonemapPass: ACES HDR → LDR (RGBA8 target for ImGui display).
        PassDesc tonemapPass;
        tonemapPass.Name       = "TonemapPass";
        tonemapPass.Pipeline   = m_TonemapPipeline;
        tonemapPass.Fullscreen = true;
        tonemapPass.ColorAttachments.push_back(m_LDRTarget);
        tonemapPass.ColorLoadOp    = ELoadOp::DontCare;
        tonemapPass.ColorStoreOp   = EStoreOp::Store;
        tonemapPass.ViewportWidth  = RenderFacade::GetViewportWidth();
        tonemapPass.ViewportHeight = RenderFacade::GetViewportHeight();
        tonemapPass.Textures.push_back({ m_HDRTarget, 0 });
        tonemapPass.Reads.push_back(m_HDRTarget);
        tonemapPass.Writes.push_back(m_LDRTarget);

        RenderFacade::GetFrameGraph().AddPass(std::move(tonemapPass));
    }

    // ============================================================================
    //  Object UBO ring buffer
    // ============================================================================

    bool RenderSystem::EnsureObjectUBO(uint32_t requiredDraws)
    {
        IRenderFactory* factory = RenderFacade::GetRenderFactory();
        if (!factory) return false;

        if (m_ObjectUBOAlignedStride == 0)
        {
            const uint32_t align = std::max<uint32_t>(1u, factory->GetUniformBufferOffsetAlignment());
            const uint32_t size  = static_cast<uint32_t>(sizeof(UBOObject));
            // Round up size to alignment.
            m_ObjectUBOAlignedStride = ((size + align - 1) / align) * align;
        }

        if (requiredDraws <= m_ObjectUBOCapacity && m_ObjectUBORing.IsValid())
            return true;

        // Grow with headroom: max(256, 2 * required).
        uint32_t newCapacity = std::max<uint32_t>(256u, requiredDraws * 2u);

        if (m_ObjectUBORing.IsValid())
        {
            factory->Delete(m_ObjectUBORing);
            m_ObjectUBORing = {};
        }

        const uint64_t totalSize = static_cast<uint64_t>(newCapacity) * m_ObjectUBOAlignedStride;
        m_ObjectUBORing = factory->CreateBuffer(
            totalSize,
            BufferUsage::Uniform,
            MemoryType::CpuToGpu,
            {},
            String("ObjectUBORing"));

        if (!m_ObjectUBORing.IsValid())
        {
            CHE_CORE_ERROR("RenderSystem::EnsureObjectUBO — failed to allocate ring ({} draws, {} bytes)",
                           newCapacity, totalSize);
            m_ObjectUBOCapacity = 0;
            return false;
        }

        m_ObjectUBOCapacity = newCapacity;
        return true;
    }

    // ============================================================================
    //  Draw list extraction
    // ============================================================================

    void RenderSystem::BuildDrawList(Scene& scene)
    {
        m_DrawList.clear();

        scene.ForEach<MeshComponent, TransformComponent, ColorComponent, VisibilityComponent>(
            [&](EntityHandle, const UUID&, MeshComponent& meshComp,
                TransformComponent& transformComp, ColorComponent& colorComp,
                VisibilityComponent& visibilityComp)
            {
                if (!visibilityComp.Visible) return;

                const Transform&  t     = transformComp.ObjectTransform;
                const glm::vec4&  color = colorComp.Color;

                const glm::mat4 model =
                    glm::translate(glm::mat4(1.0f), t.Position)
                    * glm::mat4_cast(MathUtils::QuatFromEulerDegrees(t.Rotation))
                    * glm::scale(glm::mat4(1.0f), t.Scale);

                const glm::mat4 normalMat = glm::transpose(glm::inverse(model));

                UBOObject objectUBO{};
                std::memcpy(objectUBO.Transform,     glm::value_ptr(model),      sizeof(objectUBO.Transform));
                std::memcpy(objectUBO.NormalMatrix,  glm::value_ptr(normalMat),  sizeof(objectUBO.NormalMatrix));
                objectUBO.Color[0] = color.r;
                objectUBO.Color[1] = color.g;
                objectUBO.Color[2] = color.b;
                objectUBO.Color[3] = color.a;
                objectUBO.Selected = 0.0f;

                for (Mesh& mesh : meshComp.Meshes)
                {
                    if (!mesh.Mat)
                    {
                        mesh.Mat = MaterialInstance::FromBase(
                            std::make_shared<Material>(
                                RenderFacade::GetDefaultMeshShader()));
                    }

                    const ShaderHandle shaderHandle = mesh.Mat->GetMaterial()->GetShaderHandle();
                    const BufferHandle vb = mesh.GetVertexBuffer();
                    const BufferHandle ib = mesh.GetIndexBuffer();
                    if (!vb.IsValid() || !ib.IsValid()) continue;

                    DrawItem item;
                    item.shader       = shaderHandle;
                    item.vertexBuffer = vb;
                    item.indexBuffer  = ib;
                    item.indexFormat  = mesh.GetIndexFormat();
                    item.indexCount   = mesh.GetIndexCount();
                    item.modelMatrix  = model;
                    item.object       = objectUBO;
                    item.material     = mesh.Mat;

                    m_DrawList.push_back(std::move(item));
                }
            });
    }

    // ============================================================================
    //  Camera
    // ============================================================================

    bool RenderSystem::ResolveCamera(World& world, Scene& scene, UBOCamera& out)
    {
        glm::mat4 view_proj     = glm::mat4(1.0f);
        glm::mat4 inv_view_proj = glm::mat4(1.0f);
        glm::vec3 pos           = glm::vec3(0.0f);

        if (EditorCamera* editor_camera = world.GetActiveCamera())
        {
            view_proj     = editor_camera->GetViewProjection();
            inv_view_proj = glm::inverse(view_proj);
            pos           = editor_camera->GetPosition();
        }
        else
        {
            EntityHandle cameraHandle{};
            const CameraComponent* ecs_camera = SelectActiveCamera(scene, cameraHandle);
            if (!ecs_camera)
            {
                CHE_CORE_WARN("RenderSystem: no CameraComponent found, skipping render");
                return false;
            }

            Entity* cameraEntity = scene.TryGetEntity(cameraHandle);
            if (!cameraEntity || !cameraEntity->HasComponent<TransformComponent>())
            {
                CHE_CORE_WARN("RenderSystem: camera entity has no TransformComponent, skipping render");
                return false;
            }

            const Transform& t    = cameraEntity->GetComponent<TransformComponent>().ObjectTransform;
            const glm::mat4  view = MathUtils::ViewMatrixFromTransform(t);
            const glm::mat4  proj = ecs_camera->Camera.GetProjection();

            view_proj     = proj * view;
            inv_view_proj = glm::inverse(view_proj);
            pos           = t.Position;
        }

        std::memcpy(out.ViewProjection, glm::value_ptr(view_proj),     sizeof(out.ViewProjection));
        std::memcpy(out.InvViewProj,    glm::value_ptr(inv_view_proj), sizeof(out.InvViewProj));
        out.CameraPos[0] = pos.x;
        out.CameraPos[1] = pos.y;
        out.CameraPos[2] = pos.z;
        out.CameraPos[3] = 0.0f;

        RenderFacade::SetSceneCamera(out);
        return true;
    }

    const CameraComponent* RenderSystem::SelectActiveCamera(Scene& scene, EntityHandle& outHandle)
    {
        const CameraComponent* selected = nullptr;
        EntityHandle selectedHandle{};

        scene.ForEach<CameraComponent>([&](EntityHandle handle, const UUID&, CameraComponent& camera)
            {
                if (!selected)
                {
                    selected      = &camera;
                    selectedHandle = handle;
                }

                if (camera.Primary)
                {
                    selected      = &camera;
                    selectedHandle = handle;
                }
            });

        outHandle = selectedHandle;
        return selected;
    }

    // ============================================================================
    //  Lighting
    // ============================================================================

    void RenderSystem::CollectLighting(Scene& scene, UBOLighting& out)
    {
        out.AmbientColor[0] = kDefaultAmbientR;
        out.AmbientColor[1] = kDefaultAmbientG;
        out.AmbientColor[2] = kDefaultAmbientB;
        out.AmbientColor[3] = 0.0f;

        int lightCount = 0;

        scene.ForEach<LightComponent, TransformComponent, VisibilityComponent>(
            [&](EntityHandle, const UUID&, LightComponent& lightComp,
                TransformComponent& transformComp, VisibilityComponent& visibilityComp)
            {
                if (lightCount >= MaxUBOLights) return;
                if (!visibilityComp.Visible)    return;

                const Light& light = lightComp.LightData;
                if (light.Type == LightType::None) return;

                const Transform& t = transformComp.ObjectTransform;
                UBOLightData& L = out.Lights[lightCount];

                L.Type = static_cast<int32_t>(light.Type);

                L.Position[0] = t.Position.x;
                L.Position[1] = t.Position.y;
                L.Position[2] = t.Position.z;
                L.Position[3] = 0.0f;

                const glm::vec3 dir = MathUtils::ForwardFromQuat(MathUtils::QuatFromEulerDegrees(t.Rotation));
                L.Direction[0] = dir.x;
                L.Direction[1] = dir.y;
                L.Direction[2] = dir.z;
                L.Direction[3] = 0.0f;

                L.ColorIntensity[0] = light.Color.r;
                L.ColorIntensity[1] = light.Color.g;
                L.ColorIntensity[2] = light.Color.b;
                L.ColorIntensity[3] = light.Intensity;
                L.Range     = light.Range;
                L.InnerCone = std::cos(glm::radians(light.InnerCone));
                L.OuterCone = std::cos(glm::radians(light.OuterCone));

                ++lightCount;
            });

        // Fallback directional light if no light components were found.
        if (lightCount == 0)
        {
            UBOLightData& L = out.Lights[0];
            L.Type = static_cast<int32_t>(LightType::Directional);
            L.Direction[0] = kDefaultLightDir.x;
            L.Direction[1] = kDefaultLightDir.y;
            L.Direction[2] = kDefaultLightDir.z;
            L.Direction[3] = 0.0f;
            L.ColorIntensity[0] = kDefaultLightColor.r;
            L.ColorIntensity[1] = kDefaultLightColor.g;
            L.ColorIntensity[2] = kDefaultLightColor.b;
            L.ColorIntensity[3] = kDefaultLightIntensity;
            lightCount = 1;
        }

        out.NumLights = lightCount;
    }

} // namespace CHEngine
