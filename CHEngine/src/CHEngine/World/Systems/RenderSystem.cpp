#include "chepch.h"
#include "RenderSystem.h"

#include "CHEngine/Mesh/Material.h"
#include "CHEngine/Render/RenderFacade.h"
#include "CHEngine/Scene/Components.h"
#include "CHEngine/World/World.h"
#include <Render/IShader.h>
#include <Render/UniformBlocks.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace CHEngine {

namespace {

constexpr float kDefaultAmbientR = 0.15f;
constexpr float kDefaultAmbientG = 0.15f;
constexpr float kDefaultAmbientB = 0.2f;

glm::vec3 ForwardFromEulerForLight(const glm::vec3& eulerDeg)
{
    const float pitch = glm::radians(eulerDeg.x);
    const float yaw = glm::radians(eulerDeg.y);
    return glm::normalize(glm::vec3(
        std::cos(pitch) * std::sin(yaw),
        -std::sin(pitch),
        std::cos(pitch) * std::cos(yaw)));
}

glm::vec3 ForwardFromCameraEuler(const glm::vec3& eulerDeg)
{
    const float pitch = glm::radians(eulerDeg.x);
    const float yaw = glm::radians(eulerDeg.y);
    return glm::normalize(glm::vec3(
        std::cos(yaw) * std::cos(pitch),
        std::sin(pitch),
        std::sin(yaw) * std::cos(pitch)));
}

glm::mat4 BuildViewMatrixFromCamera(const CameraComponent& camera)
{
    const glm::vec3 forward = ForwardFromCameraEuler(camera.CameraTransform.Rotation);
    glm::vec3 up(0.0f, 1.0f, 0.0f);
    if (glm::abs(forward.y) > 0.99f)
    {
        const float yaw = glm::radians(camera.CameraTransform.Rotation.y);
        up = glm::vec3(glm::cos(yaw), 0.0f, glm::sin(yaw));
    }

    return glm::lookAt(camera.CameraTransform.Position, camera.CameraTransform.Position + forward, up);
}

const CameraComponent* SelectActiveCamera(Scene& scene, EntityHandle& outHandle)
{
    const CameraComponent* selected = nullptr;
    EntityHandle selectedHandle{};

    scene.ForEach<CameraComponent>([&](EntityHandle handle, const UUID&, CameraComponent& camera) {
        if (!camera.Active)
            return;

        if (camera.Primary)
        {
            selected = &camera;
            selectedHandle = handle;
            return;
        }

        if (!selected)
        {
            selected = &camera;
            selectedHandle = handle;
        }
    });

    if (!selected)
    {
        scene.ForEach<CameraComponent>([&](EntityHandle handle, const UUID&, CameraComponent& camera) {
            if (selected)
                return;
            selected = &camera;
            selectedHandle = handle;
        });
    }

    outHandle = selectedHandle;
    return selected;
}

} // namespace

void RenderSystem::run(World& world, CommandBuffer&, Timestep)
{
    Scene& scene = world.scene();
    EntityHandle cameraHandle{};
    const CameraComponent* camera = SelectActiveCamera(scene, cameraHandle);
    if (!camera)
    {
        CHE_CORE_WARN("RenderSystem: no CameraComponent found, skipping object pass");
        return;
    }

    const Transform* cameraTransform = &camera->CameraTransform;
    if (const auto* transformComponent = scene.TryGetComponent<TransformComponent>(cameraHandle))
        cameraTransform = &transformComponent->ObjectTransform;

    const float fov = glm::max(camera->FOV, 1.0f);
    const float nearClip = glm::max(camera->NearClip, 0.0001f);
    const float farClip = glm::max(camera->FarClip, nearClip + 0.001f);

    CameraComponent cameraForView = *camera;
    cameraForView.CameraTransform = *cameraTransform;

    const float aspect = camera->AspectRatio > 0.0f ? camera->AspectRatio : (16.0f / 9.0f);

    const glm::mat4 view = BuildViewMatrixFromCamera(cameraForView);
    const glm::mat4 proj = glm::perspective(glm::radians(fov), aspect, nearClip, farClip);
    const glm::mat4 viewProj = proj * view;
    const glm::mat4 invViewProj = glm::inverse(viewProj);

    UBOCamera cameraUBO;
    std::memcpy(cameraUBO.ViewProjection, glm::value_ptr(viewProj), sizeof(cameraUBO.ViewProjection));
    std::memcpy(cameraUBO.InvViewProj, glm::value_ptr(invViewProj), sizeof(cameraUBO.InvViewProj));
    cameraUBO.CameraPos[0] = cameraTransform->Position.x;
    cameraUBO.CameraPos[1] = cameraTransform->Position.y;
    cameraUBO.CameraPos[2] = cameraTransform->Position.z;
    cameraUBO.CameraPos[3] = 0.0f;
    RenderFacade::SetSceneCamera(cameraUBO);

    UBOLighting lightingUBO;
    lightingUBO.AmbientColor[0] = kDefaultAmbientR;
    lightingUBO.AmbientColor[1] = kDefaultAmbientG;
    lightingUBO.AmbientColor[2] = kDefaultAmbientB;
    lightingUBO.AmbientColor[3] = 0.0f;

    int lightCount = 0;
    scene.ForEach<LightComponent>([&](EntityHandle handle, const UUID&, LightComponent& lightComp) {
        if (lightCount >= MaxUBOLights)
            return;

        const auto* visible = scene.TryGetComponent<VisibilityComponent>(handle);
        if (!visible || !visible->Visible)
            return;

        const auto* transformComp = scene.TryGetComponent<TransformComponent>(handle);
        if (!transformComp)
            return;

        const Light& light = lightComp.LightData;
        if (light.Type == LightType::None)
            return;

        UBOLightData& L = lightingUBO.Lights[lightCount];
        L.Type = static_cast<int32_t>(light.Type);

        const Transform& transform = transformComp->ObjectTransform;
        L.Position[0] = transform.Position.x;
        L.Position[1] = transform.Position.y;
        L.Position[2] = transform.Position.z;
        L.Position[3] = 0.0f;

        const glm::vec3 dir = ForwardFromEulerForLight(transform.Rotation);
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

    if (lightCount == 0)
    {
        UBOLightData& L = lightingUBO.Lights[0];
        L.Type = 0;
        L.Direction[0] = -0.3f;
        L.Direction[1] = -1.0f;
        L.Direction[2] = -0.5f;
        L.Direction[3] = 0.0f;
        L.ColorIntensity[0] = 1.0f;
        L.ColorIntensity[1] = 1.0f;
        L.ColorIntensity[2] = 0.95f;
        L.ColorIntensity[3] = 1.0f;
        lightCount = 1;
    }
    lightingUBO.NumLights = lightCount;

    ShaderHandle activeMeshShader{};
    scene.ForEach<MeshComponent>([&](EntityHandle handle, const UUID&, MeshComponent& meshComp) {
        const auto* visible = scene.TryGetComponent<VisibilityComponent>(handle);
        if (!visible || !visible->Visible)
            return;

        const auto* transformComp = scene.TryGetComponent<TransformComponent>(handle);
        const auto* colorComp = scene.TryGetComponent<ColorComponent>(handle);
        if (!transformComp || !colorComp)
            return;

        const Transform& transform = transformComp->ObjectTransform;
        const glm::vec4& color = colorComp->Color;

        const glm::mat4 model = glm::translate(glm::mat4(1.0f), transform.Position)
            * glm::mat4_cast(glm::quat(glm::radians(transform.Rotation)))
            * glm::scale(glm::mat4(1.0f), transform.Scale);
        const glm::mat4 normalMat = glm::transpose(glm::inverse(model));

        UBOObject objectUBO;
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
                mesh.Mat = MaterialInstance::FromBase(std::make_shared<Material>(RenderFacade::GetDefaultMeshShader()));

            const ShaderHandle shaderHandle = mesh.Mat->GetMaterial()->GetShaderHandle();
            IShader* shader = RenderFacade::GetShader(shaderHandle);
            if (!shader)
                continue;

            if (shaderHandle != activeMeshShader)
            {
                shader->Bind();
                shader->SetUniformBlock(EUniformBlock::Camera, &cameraUBO, sizeof(cameraUBO));
                shader->SetUniformBlock(EUniformBlock::Lighting, &lightingUBO, sizeof(lightingUBO));
                activeMeshShader = shaderHandle;
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
