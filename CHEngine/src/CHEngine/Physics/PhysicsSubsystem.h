#pragma once

#include "Core.h"
#include "Physics/IPhysicsFactory.h"

namespace CHEngine
{
    // Owns the physics factory pointer.  Constructed only when physics is available;
    // Application holds it as Scope<PhysicsSubsystem> (nullptr = physics disabled).
    class CHENGINE_API PhysicsSubsystem
    {
    public:
        explicit PhysicsSubsystem(IPhysicsFactory* factory);
        ~PhysicsSubsystem() = default;

        PhysicsSubsystem(const PhysicsSubsystem&) = delete;
        PhysicsSubsystem& operator=(const PhysicsSubsystem&) = delete;

        // ── World ─────────────────────────────────────────────────────────────
        IPhysicsWorld* CreateWorld(const PhysicsWorldDesc& worldDesc = {});
        void           DestroyWorld(IPhysicsWorld* world);

        // ── Shapes ────────────────────────────────────────────────────────────
        IPhysicsShape* CreateShape(const PhysicsColliderShapeDesc& shapeDesc);
        void           Delete(IPhysicsShape* shape);

    private:
        IPhysicsFactory* m_Factory = nullptr;
    };
}
