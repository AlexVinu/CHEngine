#include "chepch.h"
#include "RenderSystem.h"
#include "CHEngine/Utils/MathUtils.h"

#include "CHEngine/Mesh/Material.h"
#include "CHEngine/Render/RenderFacade.h"
#include "CHEngine/Scene/Entity.h"
#include "CHEngine/World/World.h"
#include "Log/Log.h"
#include <Render/IShader.h>
#include <Render/UniformBlocks.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace CHEngine {

    namespace {

        constexpr float kDefaultAmbientR = 0.15f;
        constexpr float kDefaultAmbientG = 0.15f;
        constexpr float kDefaultAmbientB = 0.20f;

        constexpr glm::vec3 kDefaultLightDir = { -0.3f, -1.0f, -0.5f };
        constexpr glm::vec3 kDefaultLightColor = { 1.0f,  1.0f,  0.95f };
        constexpr float     kDefaultLightIntensity = 1.0f;

    } // namespace

    // ============================================================================
    //  Public entry point
    // ============================================================================

    void RenderSystem::Run(World& world, DeferredOps& /*deferred_ops*/, Timestep /*ts*/)
    {
        auto* scene = world.GetScene();

        UBOCamera cameraUBO{};
        if (!ResolveCamera(world, *scene, cameraUBO))
            return;

        UBOLighting lightingUBO{};
        CollectLighting(*scene, lightingUBO);

        RenderMeshes(*scene, cameraUBO, lightingUBO);
    }

    // ============================================================================
    //  Camera
    // ============================================================================

    bool RenderSystem::ResolveCamera(World& world, Scene& scene, UBOCamera& out)
    {
        glm::mat4 view_proj = glm::mat4(1.0f);
        glm::mat4 inv_view_proj = glm::mat4(1.0f);
        glm::vec3 pos = glm::vec3(0.0f);

        if (EditorCamera* editor_camera = world.GetActiveCamera())
        {
            view_proj = editor_camera->GetViewProjection();
            inv_view_proj = glm::inverse(view_proj);
            pos = editor_camera->GetPosition();
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

            const Transform& t = cameraEntity->GetComponent<TransformComponent>().ObjectTransform;
            const glm::mat4 view = MathUtils::ViewMatrixFromTransform(t);
            const glm::mat4 proj = ecs_camera->Camera.GetProjection();

            view_proj = proj * view;
            inv_view_proj = glm::inverse(view_proj);
            pos = t.Position;
        }

        std::memcpy(out.ViewProjection, glm::value_ptr(view_proj), sizeof(out.ViewProjection));
        std::memcpy(out.InvViewProj, glm::value_ptr(inv_view_proj), sizeof(out.InvViewProj));
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
                    selected = &camera;
                    selectedHandle = handle;
                }

                if (camera.Primary)
                {
                    selected = &camera;
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
                if (lightCount >= MaxUBOLights)   return;
                if (!visibilityComp.Visible)      return;

                const Light& light = lightComp.LightData;
                if (light.Type == LightType::None) return;

                UBOLightData& L = out.Lights[lightCount];
                L.Type = static_cast<int32_t>(light.Type);

                const Transform& t = transformComp.ObjectTransform;
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
                L.Range = light.Range;
                L.InnerCone = std::cos(glm::radians(light.InnerCone));
                L.OuterCone = std::cos(glm::radians(light.OuterCone));

                ++lightCount;
            });

        // Fallback Ч дефолтный направленный свет если источников нет
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

    // ============================================================================
    //  Mesh rendering
    // ============================================================================

    void RenderSystem::RenderMeshes(Scene& scene, const UBOCamera& cameraUBO, const UBOLighting& lightingUBO)
    {
        m_ActiveShader = ShaderHandle{};

        scene.ForEach<MeshComponent, TransformComponent, ColorComponent, VisibilityComponent>(
            [&](EntityHandle, const UUID&, MeshComponent& meshComp,
                TransformComponent& transformComp, ColorComponent& colorComp,
                VisibilityComponent& visibilityComp)
            {
                if (!visibilityComp.Visible) return;
                const Transform& t = transformComp.ObjectTransform;
                const glm::vec4& color = colorComp.Color;

                const glm::mat4 model = glm::translate(glm::mat4(1.0f), t.Position)
                    * glm::mat4_cast(MathUtils::QuatFromEulerDegrees(t.Rotation))
                    * glm::scale(glm::mat4(1.0f), t.Scale);
                const glm::mat4 normalMat = glm::transpose(glm::inverse(model));

                UBOObject objectUBO{};
                std::memcpy(objectUBO.Transform, glm::value_ptr(model), sizeof(objectUBO.Transform));
                std::memcpy(objectUBO.NormalMatrix, glm::value_ptr(normalMat), sizeof(objectUBO.NormalMatrix));
                objectUBO.Color[0] = color.r;
                objectUBO.Color[1] = color.g;
                objectUBO.Color[2] = color.b;
                objectUBO.Color[3] = color.a;
                objectUBO.Selected = 0.0f;

                for (Mesh& mesh : meshComp.Meshes)
                {
                    if (!mesh.Mat)
                        mesh.Mat = MaterialInstance::FromBase(
                            std::make_shared<Material>(RenderFacade::GetDefaultMeshShader()));

                    const ShaderHandle shaderHandle = mesh.Mat->GetMaterial()->GetShaderHandle();
                    IShader* shader = RenderFacade::GetShader(shaderHandle);
                    if (!shader) continue;

                    if (shaderHandle != m_ActiveShader)
                    {
                        shader->Bind();
                        shader->SetUniformBlock(EUniformBlock::Camera, &cameraUBO, sizeof(cameraUBO));
                        shader->SetUniformBlock(EUniformBlock::Lighting, &lightingUBO, sizeof(lightingUBO));
                        m_ActiveShader = shaderHandle;
                    }

                    shader->SetUniformBlock(EUniformBlock::Object, &objectUBO, sizeof(objectUBO));
                    mesh.Mat->ApplyMaterial();

                    const VertexArrayHandle vao = mesh.GetVertexArray();
                    if (vao.IsValid())
                        RenderFacade::Submit(vao, model);
                }
            });
    }

} // namespace CHEngine