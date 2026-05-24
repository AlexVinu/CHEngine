#pragma once

#include "Core.h"
#include "Physics/IPhysicsFactory.h"

namespace CHEngine
{
    // Тонкий прокси над IPhysicsFactory. Application держит как Scope<PhysicsSubsystem>
    // (nullptr = физика отключена). Тяжёлые операции по handle'ам идут напрямую через
    // GetFactory() — подсистема не дублирует весь API фабрики.
    class CHENGINE_API PhysicsSubsystem
    {
    public:
        explicit PhysicsSubsystem(IPhysicsFactory* factory);
        ~PhysicsSubsystem() = default;

        PhysicsSubsystem(const PhysicsSubsystem&) = delete;
        PhysicsSubsystem& operator=(const PhysicsSubsystem&) = delete;

        // ── World ─────────────────────────────────────────────────────────────
        PhysWorldHandle CreateWorld(const PhysicsWorldDesc& worldDesc = {});
        void            DestroyWorld(PhysWorldHandle world);

        // ── Shapes ────────────────────────────────────────────────────────────
        // POD-десc из RigidBody3DComponent конвертируется внутри в variant PhysShapeDesc.
        PhysShapeHandle CreateShape(const PhysicsColliderShapeDesc& shapeDesc);
        void            Delete(PhysShapeHandle shape);

        // ── Прямой доступ к фабрике для операций над телами / queries ─────────
        IPhysicsFactory* GetFactory() { return m_Factory; }
        const IPhysicsFactory* GetFactory() const { return m_Factory; }

    private:
        IPhysicsFactory* m_Factory = nullptr;
    };
}
