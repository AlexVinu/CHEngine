#pragma once

#include "Physics/IPhysicsFactory.h"

#include <PxPhysicsAPI.h>
#include <glm/glm.hpp>

#include <memory>
#include <unordered_map>
#include <vector>

namespace CHModules
{

class PhysicsFactoryPhysX : public CHEngine::IPhysicsFactory
{
public:
    PhysicsFactoryPhysX();
    ~PhysicsFactoryPhysX() override;

    // ── World ─────────────────────────────────────────────────────────────────
    CHEngine::PhysWorldHandle CreateWorld(const CHEngine::PhysicsWorldDesc& worldDesc) override;
    void Delete(CHEngine::PhysWorldHandle world) override;

    void SetGravity(CHEngine::PhysWorldHandle world, const glm::vec3& gravity) override;
    void StepSimulation(CHEngine::PhysWorldHandle world, CHEngine::Timestep deltaTime) override;
    void SetContactListener(CHEngine::PhysWorldHandle world, CHEngine::IPhysicsContactListener* listener) override;

    // ── Shapes ────────────────────────────────────────────────────────────────
    CHEngine::PhysShapeHandle CreateShape(const CHEngine::PhysShapeDesc& shapeDesc) override;
    void Delete(CHEngine::PhysShapeHandle shape) override;

    // ── Bodies ────────────────────────────────────────────────────────────────
    CHEngine::PhysBodyHandle CreateRigidBody(CHEngine::PhysWorldHandle world,
                                             const CHEngine::PhysicsRigidBodyDesc& bodyDesc,
                                             CHEngine::PhysShapeHandle shape) override;
    void Delete(CHEngine::PhysBodyHandle body) override;

    CHEngine::PhysicsBodyType  GetBodyType(CHEngine::PhysBodyHandle body) const override;
    CHEngine::PhysicsTransform GetBodyTransform(CHEngine::PhysBodyHandle body) const override;
    void                       SetBodyTransform(CHEngine::PhysBodyHandle body, const CHEngine::PhysicsTransform& t) override;
    void                       SetBodyKinematicTarget(CHEngine::PhysBodyHandle body, const CHEngine::PhysicsTransform& t) override;
    glm::vec3                  GetBodyLinearVelocity(CHEngine::PhysBodyHandle body) const override;
    void                       SetBodyLinearVelocity(CHEngine::PhysBodyHandle body, const glm::vec3& velocity) override;
    void                       AddBodyForce(CHEngine::PhysBodyHandle body, const glm::vec3& force, CHEngine::PhysicsForceMode mode) override;

    // ── Queries ───────────────────────────────────────────────────────────────
    bool Raycast(CHEngine::PhysWorldHandle world, const glm::vec3& origin, const glm::vec3& direction,
                 float maxDistance, CHEngine::PhysicsRaycastHit& outHit) override;
    bool OverlapSphere(CHEngine::PhysWorldHandle world, const glm::vec3& center, float radius,
                       CHEngine::PhysicsOverlapResult& outResult) override;
    bool OverlapBox(CHEngine::PhysWorldHandle world, const glm::vec3& center, const glm::vec3& halfExtents,
                    const glm::quat& rotation, CHEngine::PhysicsOverlapResult& outResult) override;
    bool OverlapCapsule(CHEngine::PhysWorldHandle world, const glm::vec3& center, float radius, float halfHeight,
                        const glm::quat& rotation, CHEngine::PhysicsOverlapResult& outResult) override;

    // ── Module info ───────────────────────────────────────────────────────────
    CHEngine::EPhysicsAPI GetPhysicsApi() override;
    bool                  CheckIsWorking() override;
    CHEngine::ModuleType  GetType() const override;

private:
    // ── Internal shape type (not exposed via handle API) ─────────────────────
    enum class PhysXShapeType : uint8_t { Box = 0, Sphere, Capsule };

    // ── Per-world contact callback (one instance per WorldRecord) ─────────────
    struct ContactCallbackPhysX;   // forward-declared so WorldRecord can hold unique_ptr

    // ── Native stores ─────────────────────────────────────────────────────────
    struct WorldRecord
    {
        physx::PxScene*                        scene      = nullptr;
        physx::PxDefaultCpuDispatcher*         dispatcher = nullptr;
        CHEngine::IPhysicsContactListener*     listener   = nullptr;
        std::unique_ptr<ContactCallbackPhysX>  contactCb;
        uint32_t generation = 0;
        bool     alive      = false;
    };

    struct ShapeRecord
    {
        PhysXShapeType type       = PhysXShapeType::Box;
        glm::vec3 halfExtents     { 0.5f, 0.5f, 0.5f };
        float     radius          = 0.5f;
        float     halfHeight      = 0.5f;
        uint32_t  generation      = 0;
        bool      alive           = false;
    };

    struct BodyRecord
    {
        physx::PxRigidActor*      actor = nullptr;
        CHEngine::PhysWorldHandle world{};
        CHEngine::PhysicsBodyType type  = CHEngine::PhysicsBodyType::Dynamic;
        uint32_t generation = 0;
        bool     alive      = false;
    };

    // Определение ContactCallbackPhysX после WorldRecord, чтобы unique_ptr мог быть инстанциирован.
    struct ContactCallbackPhysX final : physx::PxSimulationEventCallback
    {
        PhysicsFactoryPhysX* factory    = nullptr;
        uint32_t             worldIndex = 0;

        void onContact(const physx::PxContactPairHeader& pairHeader,
                       const physx::PxContactPair* pairs,
                       physx::PxU32 nbPairs) override;

        void onConstraintBreak(physx::PxConstraintInfo*, physx::PxU32)                                        override {}
        void onWake(physx::PxActor**, physx::PxU32)                                                            override {}
        void onSleep(physx::PxActor**, physx::PxU32)                                                           override {}
        void onTrigger(physx::PxTriggerPair*, physx::PxU32)                                                    override {}
        void onAdvance(const physx::PxRigidBody* const*, const physx::PxTransform*, physx::PxU32)              override {}
    };

    // ── Allocation / resolution helpers ───────────────────────────────────────
    CHEngine::PhysWorldHandle AllocWorldSlot(physx::PxScene* scene,
                                             physx::PxDefaultCpuDispatcher* dispatcher,
                                             std::unique_ptr<ContactCallbackPhysX> cb);
    CHEngine::PhysShapeHandle AllocShapeSlot(PhysXShapeType type,
                                             const glm::vec3& halfExtents,
                                             float radius, float halfHeight);
    CHEngine::PhysBodyHandle  AllocBodySlot(physx::PxRigidActor* actor,
                                             CHEngine::PhysWorldHandle world,
                                             CHEngine::PhysicsBodyType type);

    WorldRecord* ResolveWorld(CHEngine::PhysWorldHandle h);
    const WorldRecord* ResolveWorld(CHEngine::PhysWorldHandle h) const;
    const ShapeRecord* ResolveShape(CHEngine::PhysShapeHandle h) const;
    BodyRecord*        ResolveBody(CHEngine::PhysBodyHandle h);
    const BodyRecord*  ResolveBody(CHEngine::PhysBodyHandle h) const;

    // Обратный индекс actor* → handle (нужен для contact callbacks и query results).
    CHEngine::PhysBodyHandle HandleForActor(physx::PxRigidActor* actor) const;

    // Удаляет actor из сцены и освобождает его (вызывается при Delete(PhysBodyHandle)).
    void ReleaseBodyActor(BodyRecord& rec);

    // ── PhysX core ────────────────────────────────────────────────────────────
    physx::PxDefaultAllocator     m_Allocator;
    physx::PxDefaultErrorCallback m_ErrorCallback;
    physx::PxFoundation*          m_Foundation = nullptr;
    physx::PxPhysics*             m_Physics    = nullptr;

    // ── Stores ────────────────────────────────────────────────────────────────
    std::vector<WorldRecord> m_Worlds;
    std::vector<uint32_t>    m_FreeWorldSlots;

    std::vector<ShapeRecord> m_Shapes;
    std::vector<uint32_t>    m_FreeShapeSlots;

    std::vector<BodyRecord>  m_Bodies;
    std::vector<uint32_t>    m_FreeBodySlots;

    // Обратный индекс PxRigidActor* → PhysBodyHandle для callbacks/queries.
    std::unordered_map<physx::PxRigidActor*, CHEngine::PhysBodyHandle> m_BodyByActor;
};

} // namespace CHModules
