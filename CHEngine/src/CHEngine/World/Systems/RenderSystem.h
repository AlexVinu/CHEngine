#pragma once
#include "CHEngine/Scene/Scene.h"
#include "CHEngine/Scene/Components.h"
#include "CHEngine/World/ISystem.h"
#include "Render/UniformBlocks.h"
#include "CHEngine/Camera/EditorCamera.h"

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

    private:
        // «аполн€ет cameraUBO, возвращает false если камера не найдена
        bool ResolveCamera(World& world, Scene& scene, UBOCamera& out);

        // —обирает все источники света, пишет fallback если нет ни одного
        void CollectLighting(Scene& scene, UBOLighting& out);

        // –ендерит все видимые меши
        void RenderMeshes(Scene& scene, const UBOCamera& cameraUBO, const UBOLighting& lightingUBO);

        // »щет активный CameraComponent в ECS (Primary или первый попавшийс€)
        const CameraComponent* SelectActiveCamera(Scene& scene, EntityHandle& outHandle);

    private:
        ShaderHandle m_ActiveShader{};
    };

} // namespace CHEngine