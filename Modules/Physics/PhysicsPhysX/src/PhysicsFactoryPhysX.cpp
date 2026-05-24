#include "chepch.h"

#include "PhysicsFactoryPhysX.h"

#include <extensions/PxExtensionsAPI.h>
#include <algorithm>
#include <glm/gtc/quaternion.hpp>

// ─────────────────────────────────────────────────────────────────────────────
// Conversion helpers
// ─────────────────────────────────────────────────────────────────────────────
namespace
{
    physx::PxVec3 ToPxVec3(const glm::vec3& v)
    {
        return { v.x, v.y, v.z };
    }

    glm::vec3 FromPxVec3(const physx::PxVec3& v)
    {
        return { v.x, v.y, v.z };
    }

    physx::PxQuat ToPxQuat(const glm::quat& q)
    {
        return { q.x, q.y, q.z, q.w };
    }

    glm::quat FromPxQuat(const physx::PxQuat& q)
    {
        return glm::quat(q.w, q.x, q.y, q.z);
    }

    physx::PxTransform ToPxTransform(const glm::vec3& position, const glm::quat& rotation)
    {
        return { ToPxVec3(position), ToPxQuat(rotation) };
    }

    physx::PxTransform ToPxTransform(const CHEngine::PhysicsTransform& t)
    {
        return { ToPxVec3(t.Position), ToPxQuat(t.Rotation) };
    }

    CHEngine::PhysicsTransform FromPxTransform(const physx::PxTransform& t)
    {
        CHEngine::PhysicsTransform out{};
        out.Position = FromPxVec3(t.p);
        out.Rotation = FromPxQuat(t.q);
        return out;
    }

    physx::PxForceMode::Enum ToPxForceMode(CHEngine::PhysicsForceMode mode)
    {
        switch (mode)
        {
            case CHEngine::PhysicsForceMode::Impulse:        return physx::PxForceMode::eIMPULSE;
            case CHEngine::PhysicsForceMode::VelocityChange: return physx::PxForceMode::eVELOCITY_CHANGE;
            case CHEngine::PhysicsForceMode::Acceleration:   return physx::PxForceMode::eACCELERATION;
            default:                                          return physx::PxForceMode::eFORCE;
        }
    }
}

namespace CHModules
{

// ─────────────────────────────────────────────────────────────────────────────
// ContactCallbackPhysX::onContact
// ─────────────────────────────────────────────────────────────────────────────
void PhysicsFactoryPhysX::ContactCallbackPhysX::onContact(
    const physx::PxContactPairHeader& pairHeader,
    const physx::PxContactPair* pairs,
    physx::PxU32 nbPairs)
{
    if (!factory) return;

    auto& worldRec = factory->m_Worlds[worldIndex];
    if (!worldRec.alive || !worldRec.listener) return;

    auto* actorA = pairHeader.actors[0]->is<physx::PxRigidActor>();
    auto* actorB = pairHeader.actors[1]->is<physx::PxRigidActor>();
    const auto handleA = factory->HandleForActor(actorA);
    const auto handleB = factory->HandleForActor(actorB);

    for (physx::PxU32 i = 0; i < nbPairs; ++i)
    {
        physx::PxContactPairPoint contactPoints[8];
        const physx::PxU32 pointCount = pairs[i].extractContacts(contactPoints, 8);
        if (pointCount == 0) continue;

        CHEngine::PhysicsContactEvent eventData{};
        eventData.BodyA  = handleA;
        eventData.BodyB  = handleB;
        eventData.Point  = FromPxVec3(contactPoints[0].position);
        eventData.Normal = FromPxVec3(contactPoints[0].normal);
        worldRec.listener->OnContact(eventData);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────────
PhysicsFactoryPhysX::PhysicsFactoryPhysX()
{
    m_Foundation = PxCreateFoundation(PX_PHYSICS_VERSION, m_Allocator, m_ErrorCallback);
    if (!m_Foundation)
    {
        CHE_CORE_ERROR("PhysicsFactoryPhysX: failed to create PxFoundation");
        return;
    }

    physx::PxTolerancesScale scale;
    m_Physics = PxCreatePhysics(PX_PHYSICS_VERSION, *m_Foundation, scale, false, nullptr);
    if (!m_Physics)
    {
        CHE_CORE_ERROR("PhysicsFactoryPhysX: failed to create PxPhysics");
        return;
    }

    if (!PxInitExtensions(*m_Physics, nullptr))
        CHE_CORE_ERROR("PhysicsFactoryPhysX: PxInitExtensions failed");
}

PhysicsFactoryPhysX::~PhysicsFactoryPhysX()
{
    // Тела принадлежат сценам — при scene->release() тела освобождаются каскадно.
    // Очищаем только метаданные reverse-map и слоты.
    m_BodyByActor.clear();
    m_Bodies.clear();
    m_FreeBodySlots.clear();

    // Удаляем миры (cascade: scene->release() освобождает все акторы).
    for (auto& rec : m_Worlds)
    {
        if (!rec.alive) continue;

        rec.contactCb.reset();   // отключаем callback до release сцены

        if (rec.scene)
        {
            rec.scene->release();
            rec.scene = nullptr;
        }
        if (rec.dispatcher)
        {
            rec.dispatcher->release();
            rec.dispatcher = nullptr;
        }
        rec.alive = false;
    }
    m_Worlds.clear();

    // Шейпы — чистые value-дескрипторы, PxShape не создаётся до привязки к body.
    m_Shapes.clear();

    if (m_Physics)
    {
        PxCloseExtensions();
        m_Physics->release();
        m_Physics = nullptr;
    }
    if (m_Foundation)
    {
        m_Foundation->release();
        m_Foundation = nullptr;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Allocation helpers
// ─────────────────────────────────────────────────────────────────────────────
CHEngine::PhysWorldHandle PhysicsFactoryPhysX::AllocWorldSlot(physx::PxScene* scene,
                                                               physx::PxDefaultCpuDispatcher* dispatcher,
                                                               std::unique_ptr<ContactCallbackPhysX> cb)
{
    uint32_t index;
    if (!m_FreeWorldSlots.empty())
    {
        index = m_FreeWorldSlots.back();
        m_FreeWorldSlots.pop_back();
        auto& rec    = m_Worlds[index];
        rec.scene      = scene;
        rec.dispatcher = dispatcher;
        rec.listener   = nullptr;
        rec.contactCb  = std::move(cb);
        rec.alive      = true;
    }
    else
    {
        index = static_cast<uint32_t>(m_Worlds.size());
        WorldRecord rec{};
        rec.scene      = scene;
        rec.dispatcher = dispatcher;
        rec.contactCb  = std::move(cb);
        rec.alive      = true;
        m_Worlds.push_back(std::move(rec));
    }
    return CHEngine::PhysWorldHandle{ index, m_Worlds[index].generation };
}

CHEngine::PhysShapeHandle PhysicsFactoryPhysX::AllocShapeSlot(PhysXShapeType type,
                                                               const glm::vec3& halfExtents,
                                                               float radius, float halfHeight)
{
    uint32_t index;
    if (!m_FreeShapeSlots.empty())
    {
        index = m_FreeShapeSlots.back();
        m_FreeShapeSlots.pop_back();
        auto& rec       = m_Shapes[index];
        rec.type        = type;
        rec.halfExtents = halfExtents;
        rec.radius      = radius;
        rec.halfHeight  = halfHeight;
        rec.alive       = true;
    }
    else
    {
        index = static_cast<uint32_t>(m_Shapes.size());
        ShapeRecord rec{};
        rec.type        = type;
        rec.halfExtents = halfExtents;
        rec.radius      = radius;
        rec.halfHeight  = halfHeight;
        rec.alive       = true;
        m_Shapes.push_back(rec);
    }
    return CHEngine::PhysShapeHandle{ index, m_Shapes[index].generation };
}

CHEngine::PhysBodyHandle PhysicsFactoryPhysX::AllocBodySlot(physx::PxRigidActor* actor,
                                                             CHEngine::PhysWorldHandle world,
                                                             CHEngine::PhysicsBodyType type)
{
    uint32_t index;
    if (!m_FreeBodySlots.empty())
    {
        index = m_FreeBodySlots.back();
        m_FreeBodySlots.pop_back();
        auto& rec  = m_Bodies[index];
        rec.actor  = actor;
        rec.world  = world;
        rec.type   = type;
        rec.alive  = true;
    }
    else
    {
        index = static_cast<uint32_t>(m_Bodies.size());
        BodyRecord rec{};
        rec.actor = actor;
        rec.world = world;
        rec.type  = type;
        rec.alive = true;
        m_Bodies.push_back(rec);
    }
    CHEngine::PhysBodyHandle h{ index, m_Bodies[index].generation };
    m_BodyByActor[actor] = h;
    return h;
}

// ─────────────────────────────────────────────────────────────────────────────
// Resolution helpers
// ─────────────────────────────────────────────────────────────────────────────
PhysicsFactoryPhysX::WorldRecord* PhysicsFactoryPhysX::ResolveWorld(CHEngine::PhysWorldHandle h)
{
    if (!h.IsValid() || h.index >= m_Worlds.size()) return nullptr;
    auto& rec = m_Worlds[h.index];
    if (!rec.alive || rec.generation != h.generation) return nullptr;
    return &rec;
}

const PhysicsFactoryPhysX::WorldRecord* PhysicsFactoryPhysX::ResolveWorld(CHEngine::PhysWorldHandle h) const
{
    if (!h.IsValid() || h.index >= m_Worlds.size()) return nullptr;
    const auto& rec = m_Worlds[h.index];
    if (!rec.alive || rec.generation != h.generation) return nullptr;
    return &rec;
}

const PhysicsFactoryPhysX::ShapeRecord* PhysicsFactoryPhysX::ResolveShape(CHEngine::PhysShapeHandle h) const
{
    if (!h.IsValid() || h.index >= m_Shapes.size()) return nullptr;
    const auto& rec = m_Shapes[h.index];
    if (!rec.alive || rec.generation != h.generation) return nullptr;
    return &rec;
}

PhysicsFactoryPhysX::BodyRecord* PhysicsFactoryPhysX::ResolveBody(CHEngine::PhysBodyHandle h)
{
    if (!h.IsValid() || h.index >= m_Bodies.size()) return nullptr;
    auto& rec = m_Bodies[h.index];
    if (!rec.alive || rec.generation != h.generation) return nullptr;
    return &rec;
}

const PhysicsFactoryPhysX::BodyRecord* PhysicsFactoryPhysX::ResolveBody(CHEngine::PhysBodyHandle h) const
{
    if (!h.IsValid() || h.index >= m_Bodies.size()) return nullptr;
    const auto& rec = m_Bodies[h.index];
    if (!rec.alive || rec.generation != h.generation) return nullptr;
    return &rec;
}

CHEngine::PhysBodyHandle PhysicsFactoryPhysX::HandleForActor(physx::PxRigidActor* actor) const
{
    if (!actor) return {};
    auto it = m_BodyByActor.find(actor);
    return it != m_BodyByActor.end() ? it->second : CHEngine::PhysBodyHandle{};
}

void PhysicsFactoryPhysX::ReleaseBodyActor(BodyRecord& rec)
{
    if (!rec.actor) return;

    // Удаляем из сцены, если мир ещё жив.
    if (const auto* worldRec = ResolveWorld(rec.world))
    {
        if (worldRec->scene)
            worldRec->scene->removeActor(*rec.actor, false);
    }

    m_BodyByActor.erase(rec.actor);
    rec.actor->release();
    rec.actor = nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// World
// ─────────────────────────────────────────────────────────────────────────────
CHEngine::PhysWorldHandle PhysicsFactoryPhysX::CreateWorld(const CHEngine::PhysicsWorldDesc& worldDesc)
{
    if (!m_Physics) return {};

    auto* dispatcher = physx::PxDefaultCpuDispatcherCreate(std::max(1u, worldDesc.SolverThreads));
    if (!dispatcher)
    {
        CHE_CORE_ERROR("PhysicsFactoryPhysX: failed to create cpu dispatcher");
        return {};
    }

    // Аллоцируем индекс заранее, чтобы ContactCallbackPhysX знал его.
    uint32_t index;
    if (!m_FreeWorldSlots.empty())
    {
        index = m_FreeWorldSlots.back();
        m_FreeWorldSlots.pop_back();
    }
    else
    {
        index = static_cast<uint32_t>(m_Worlds.size());
        m_Worlds.emplace_back();  // default-constructed, alive=false пока
    }

    auto cb = std::make_unique<ContactCallbackPhysX>();
    cb->factory    = this;
    cb->worldIndex = index;

    physx::PxSceneDesc sceneDesc(m_Physics->getTolerancesScale());
    sceneDesc.gravity           = ToPxVec3(worldDesc.Gravity);
    sceneDesc.cpuDispatcher     = dispatcher;
    sceneDesc.filterShader      = physx::PxDefaultSimulationFilterShader;
    sceneDesc.simulationEventCallback = cb.get();

    physx::PxScene* scene = m_Physics->createScene(sceneDesc);
    if (!scene)
    {
        CHE_CORE_ERROR("PhysicsFactoryPhysX: failed to create PxScene");
        dispatcher->release();
        m_FreeWorldSlots.push_back(index);   // вернём слот обратно
        return {};
    }

    auto& rec      = m_Worlds[index];
    rec.scene      = scene;
    rec.dispatcher = dispatcher;
    rec.listener   = nullptr;
    rec.contactCb  = std::move(cb);
    rec.alive      = true;
    // generation остаётся как есть (уже инкрементирован при предыдущем Delete)

    return CHEngine::PhysWorldHandle{ index, rec.generation };
}

void PhysicsFactoryPhysX::Delete(CHEngine::PhysWorldHandle world)
{
    if (!world.IsValid() || world.index >= m_Worlds.size()) return;
    auto& rec = m_Worlds[world.index];
    if (!rec.alive || rec.generation != world.generation) return;

    // Инвалидируем все тела этого мира (сцена их удалит каскадно).
    for (uint32_t i = 0; i < static_cast<uint32_t>(m_Bodies.size()); ++i)
    {
        auto& bodyRec = m_Bodies[i];
        if (!bodyRec.alive || !(bodyRec.world == world)) continue;

        m_BodyByActor.erase(bodyRec.actor);
        bodyRec.actor = nullptr;   // освободится при scene->release()
        bodyRec.alive = false;
        ++bodyRec.generation;
        m_FreeBodySlots.push_back(i);
    }

    rec.contactCb.reset();

    if (rec.scene)
    {
        rec.scene->release();
        rec.scene = nullptr;
    }
    if (rec.dispatcher)
    {
        rec.dispatcher->release();
        rec.dispatcher = nullptr;
    }

    rec.alive = false;
    ++rec.generation;
    m_FreeWorldSlots.push_back(world.index);
}

void PhysicsFactoryPhysX::SetGravity(CHEngine::PhysWorldHandle world, const glm::vec3& gravity)
{
    if (auto* rec = ResolveWorld(world))
        if (rec->scene) rec->scene->setGravity(ToPxVec3(gravity));
}

void PhysicsFactoryPhysX::StepSimulation(CHEngine::PhysWorldHandle world, CHEngine::Timestep deltaTime)
{
    auto* rec = ResolveWorld(world);
    if (!rec || !rec->scene || deltaTime <= 0.0f) return;
    const physx::PxReal step = static_cast<physx::PxReal>(deltaTime.GetSeconds());
    rec->scene->simulate(step);
    rec->scene->fetchResults(true);
}

void PhysicsFactoryPhysX::SetContactListener(CHEngine::PhysWorldHandle world, CHEngine::IPhysicsContactListener* listener)
{
    if (auto* rec = ResolveWorld(world))
        rec->listener = listener;
}

// ─────────────────────────────────────────────────────────────────────────────
// Shapes
// ─────────────────────────────────────────────────────────────────────────────
CHEngine::PhysShapeHandle PhysicsFactoryPhysX::CreateShape(const CHEngine::PhysShapeDesc& shapeDesc)
{
    return std::visit([this](auto&& desc) -> CHEngine::PhysShapeHandle {
        using T = std::decay_t<decltype(desc)>;
        if constexpr (std::is_same_v<T, CHEngine::BoxDesc>)
            return AllocShapeSlot(PhysXShapeType::Box, desc.halfExtents, 0.0f, 0.0f);
        else if constexpr (std::is_same_v<T, CHEngine::SphereDesc>)
            return AllocShapeSlot(PhysXShapeType::Sphere, glm::vec3(0.0f), desc.radius, 0.0f);
        else if constexpr (std::is_same_v<T, CHEngine::CapsuleDesc>)
            return AllocShapeSlot(PhysXShapeType::Capsule, glm::vec3(0.0f), desc.radius, desc.halfHeight);
        else
            return {};
    }, shapeDesc);
}

void PhysicsFactoryPhysX::Delete(CHEngine::PhysShapeHandle shape)
{
    if (!shape.IsValid() || shape.index >= m_Shapes.size()) return;
    auto& rec = m_Shapes[shape.index];
    if (!rec.alive || rec.generation != shape.generation) return;

    // ShapeRecord — чистый дескриптор, PxShape* не создаётся здесь.
    rec.alive = false;
    ++rec.generation;
    m_FreeShapeSlots.push_back(shape.index);
}

// ─────────────────────────────────────────────────────────────────────────────
// Bodies
// ─────────────────────────────────────────────────────────────────────────────
CHEngine::PhysBodyHandle PhysicsFactoryPhysX::CreateRigidBody(CHEngine::PhysWorldHandle world,
                                                               const CHEngine::PhysicsRigidBodyDesc& bodyDesc,
                                                               CHEngine::PhysShapeHandle shape)
{
    auto* worldRec = ResolveWorld(world);
    auto* shapeRec = ResolveShape(shape);
    if (!worldRec || !shapeRec || !worldRec->scene || !m_Physics) return {};

    // Строим PxGeometry из дескриптора шейпа.
    physx::PxGeometryHolder geometryHolder;
    switch (shapeRec->type)
    {
        case PhysXShapeType::Box:
            geometryHolder.storeAny(physx::PxBoxGeometry(ToPxVec3(shapeRec->halfExtents)));
            break;
        case PhysXShapeType::Sphere:
            geometryHolder.storeAny(physx::PxSphereGeometry(shapeRec->radius));
            break;
        case PhysXShapeType::Capsule:
            geometryHolder.storeAny(physx::PxCapsuleGeometry(shapeRec->radius, shapeRec->halfHeight));
            break;
        default:
            return {};
    }

    physx::PxMaterial* material = m_Physics->createMaterial(
        bodyDesc.StaticFriction, bodyDesc.DynamicFriction, bodyDesc.Restitution);
    if (!material) return {};

    const physx::PxTransform pxTransform = ToPxTransform(bodyDesc.Transform);
    physx::PxRigidActor* actor = nullptr;

    if (bodyDesc.Type == CHEngine::PhysicsBodyType::Static)
    {
        actor = m_Physics->createRigidStatic(pxTransform);
    }
    else
    {
        auto* dynamicActor = m_Physics->createRigidDynamic(pxTransform);
        if (dynamicActor)
        {
            dynamicActor->setLinearDamping(bodyDesc.LinearDamping);
            dynamicActor->setAngularDamping(bodyDesc.AngularDamping);
            dynamicActor->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, !bodyDesc.EnableGravity);

            if (bodyDesc.Type == CHEngine::PhysicsBodyType::Kinematic)
                dynamicActor->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, true);
        }
        actor = dynamicActor;
    }

    if (!actor)
    {
        material->release();
        return {};
    }

    physx::PxShape* pxShape = m_Physics->createShape(geometryHolder.any(), *material, true);
    material->release();
    if (!pxShape)
    {
        actor->release();
        return {};
    }

    actor->attachShape(*pxShape);
    pxShape->release();

    if (auto* dynamicActor = actor->is<physx::PxRigidDynamic>())
        physx::PxRigidBodyExt::setMassAndUpdateInertia(*dynamicActor, std::max(0.001f, bodyDesc.Mass));

    worldRec->scene->addActor(*actor);

    return AllocBodySlot(actor, world, bodyDesc.Type);
}

void PhysicsFactoryPhysX::Delete(CHEngine::PhysBodyHandle body)
{
    auto* rec = ResolveBody(body);
    if (!rec) return;

    ReleaseBodyActor(*rec);
    rec->alive = false;
    ++rec->generation;
    m_FreeBodySlots.push_back(body.index);
}

CHEngine::PhysicsBodyType PhysicsFactoryPhysX::GetBodyType(CHEngine::PhysBodyHandle body) const
{
    if (const auto* rec = ResolveBody(body)) return rec->type;
    return CHEngine::PhysicsBodyType::Static;
}

CHEngine::PhysicsTransform PhysicsFactoryPhysX::GetBodyTransform(CHEngine::PhysBodyHandle body) const
{
    if (const auto* rec = ResolveBody(body))
        if (rec->actor) return FromPxTransform(rec->actor->getGlobalPose());
    return {};
}

void PhysicsFactoryPhysX::SetBodyTransform(CHEngine::PhysBodyHandle body, const CHEngine::PhysicsTransform& t)
{
    if (auto* rec = ResolveBody(body))
        if (rec->actor) rec->actor->setGlobalPose(ToPxTransform(t), true);
}

void PhysicsFactoryPhysX::SetBodyKinematicTarget(CHEngine::PhysBodyHandle body, const CHEngine::PhysicsTransform& t)
{
    if (auto* rec = ResolveBody(body))
    {
        if (!rec->actor) return;
        auto* dyn = rec->actor->is<physx::PxRigidDynamic>();
        if (dyn) dyn->setKinematicTarget(ToPxTransform(t));
    }
}

glm::vec3 PhysicsFactoryPhysX::GetBodyLinearVelocity(CHEngine::PhysBodyHandle body) const
{
    if (const auto* rec = ResolveBody(body))
    {
        if (!rec->actor) return {};
        auto* dyn = rec->actor->is<physx::PxRigidDynamic>();
        if (dyn) return FromPxVec3(dyn->getLinearVelocity());
    }
    return {};
}

void PhysicsFactoryPhysX::SetBodyLinearVelocity(CHEngine::PhysBodyHandle body, const glm::vec3& velocity)
{
    if (auto* rec = ResolveBody(body))
    {
        if (!rec->actor) return;
        auto* dyn = rec->actor->is<physx::PxRigidDynamic>();
        if (dyn) dyn->setLinearVelocity(ToPxVec3(velocity));
    }
}

void PhysicsFactoryPhysX::AddBodyForce(CHEngine::PhysBodyHandle body, const glm::vec3& force, CHEngine::PhysicsForceMode mode)
{
    if (auto* rec = ResolveBody(body))
    {
        if (!rec->actor) return;
        auto* dyn = rec->actor->is<physx::PxRigidDynamic>();
        if (dyn) dyn->addForce(ToPxVec3(force), ToPxForceMode(mode));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Queries
// ─────────────────────────────────────────────────────────────────────────────
bool PhysicsFactoryPhysX::Raycast(CHEngine::PhysWorldHandle world, const glm::vec3& origin,
                                   const glm::vec3& direction, float maxDistance,
                                   CHEngine::PhysicsRaycastHit& outHit)
{
    outHit = {};
    const auto* rec = ResolveWorld(world);
    if (!rec || !rec->scene || maxDistance <= 0.0f) return false;

    physx::PxVec3 dir = ToPxVec3(direction);
    if (dir.magnitudeSquared() <= 0.000001f) return false;
    dir.normalize();

    physx::PxRaycastBuffer hitBuffer;
    if (!rec->scene->raycast(ToPxVec3(origin), dir, maxDistance, hitBuffer))
        return false;

    outHit.HasHit   = true;
    outHit.Distance = hitBuffer.block.distance;
    outHit.Position = FromPxVec3(hitBuffer.block.position);
    outHit.Normal   = FromPxVec3(hitBuffer.block.normal);
    outHit.Body     = HandleForActor(hitBuffer.block.actor);
    return true;
}

bool PhysicsFactoryPhysX::OverlapSphere(CHEngine::PhysWorldHandle world, const glm::vec3& center, float radius,
                                         CHEngine::PhysicsOverlapResult& outResult)
{
    outResult = {};
    const auto* rec = ResolveWorld(world);
    if (!rec || !rec->scene || radius <= 0.0f) return false;

    constexpr physx::PxU32 MaxHits = 64;
    physx::PxOverlapBufferN<MaxHits> buf;
    const physx::PxSphereGeometry geom(radius);
    if (!rec->scene->overlap(geom, ToPxTransform(center, glm::quat(1.0f, 0.0f, 0.0f, 0.0f)), buf))
        return false;

    auto append = [&](const physx::PxOverlapHit& h) {
        CHEngine::PhysicsOverlapHit hit{};
        hit.Body = HandleForActor(h.actor);
        outResult.Hits.push_back(hit);
    };
    if (buf.hasBlock)  append(buf.block);
    for (physx::PxU32 i = 0; i < buf.nbTouches; ++i) append(buf.touches[i]);
    outResult.HasHit = !outResult.Hits.empty();
    return outResult.HasHit;
}

bool PhysicsFactoryPhysX::OverlapBox(CHEngine::PhysWorldHandle world, const glm::vec3& center,
                                      const glm::vec3& halfExtents, const glm::quat& rotation,
                                      CHEngine::PhysicsOverlapResult& outResult)
{
    outResult = {};
    const auto* rec = ResolveWorld(world);
    if (!rec || !rec->scene) return false;
    if (halfExtents.x <= 0.0f || halfExtents.y <= 0.0f || halfExtents.z <= 0.0f) return false;

    constexpr physx::PxU32 MaxHits = 64;
    physx::PxOverlapBufferN<MaxHits> buf;
    const physx::PxBoxGeometry geom(ToPxVec3(halfExtents));
    if (!rec->scene->overlap(geom, ToPxTransform(center, rotation), buf))
        return false;

    auto append = [&](const physx::PxOverlapHit& h) {
        CHEngine::PhysicsOverlapHit hit{};
        hit.Body = HandleForActor(h.actor);
        outResult.Hits.push_back(hit);
    };
    if (buf.hasBlock)  append(buf.block);
    for (physx::PxU32 i = 0; i < buf.nbTouches; ++i) append(buf.touches[i]);
    outResult.HasHit = !outResult.Hits.empty();
    return outResult.HasHit;
}

bool PhysicsFactoryPhysX::OverlapCapsule(CHEngine::PhysWorldHandle world, const glm::vec3& center,
                                          float radius, float halfHeight, const glm::quat& rotation,
                                          CHEngine::PhysicsOverlapResult& outResult)
{
    outResult = {};
    const auto* rec = ResolveWorld(world);
    if (!rec || !rec->scene || radius <= 0.0f || halfHeight <= 0.0f) return false;

    constexpr physx::PxU32 MaxHits = 64;
    physx::PxOverlapBufferN<MaxHits> buf;
    const physx::PxCapsuleGeometry geom(radius, halfHeight);
    if (!rec->scene->overlap(geom, ToPxTransform(center, rotation), buf))
        return false;

    auto append = [&](const physx::PxOverlapHit& h) {
        CHEngine::PhysicsOverlapHit hit{};
        hit.Body = HandleForActor(h.actor);
        outResult.Hits.push_back(hit);
    };
    if (buf.hasBlock)  append(buf.block);
    for (physx::PxU32 i = 0; i < buf.nbTouches; ++i) append(buf.touches[i]);
    outResult.HasHit = !outResult.Hits.empty();
    return outResult.HasHit;
}

// ─────────────────────────────────────────────────────────────────────────────
// Module info
// ─────────────────────────────────────────────────────────────────────────────
CHEngine::EPhysicsAPI PhysicsFactoryPhysX::GetPhysicsApi()
{
    return CHEngine::EPhysicsAPI::PhysX;
}

bool PhysicsFactoryPhysX::CheckIsWorking()
{
    return m_Foundation != nullptr && m_Physics != nullptr;
}

CHEngine::ModuleType PhysicsFactoryPhysX::GetType() const
{
    return CHEngine::ModuleType::Physics;
}

} // namespace CHModules

IMPLEMENT_MODULE_FACTORY(CHModules::PhysicsFactoryPhysX)
