//#include "chepch.h"
//#include "TransformDirtySystem.h"
//
//#include "CHEngine/Scene/Entity.h"
//
//#include <cmath>
//#include <unordered_set>
//#include <vector>
//
//namespace CHEngine {
//
//namespace {
//constexpr float kTransformEpsilon = 0.0001f;
//
//bool IsNearlyEqualVec3(const glm::vec3& left, const glm::vec3& right)
//{
//    return std::fabs(left.x - right.x) <= kTransformEpsilon
//        && std::fabs(left.y - right.y) <= kTransformEpsilon
//        && std::fabs(left.z - right.z) <= kTransformEpsilon;
//}
//} // namespace
//
//void TransformDirtySystem::run(Scene& scene, CommandBuffer& commands, Timestep)
//{
//    std::unordered_set<UUID, boost::hash<UUID>> seen;
//    scene.ForEach<TransformComponent>([&](EntityHandle handle, const UUID& uuid, TransformComponent& transformComponent) {
//        seen.insert(uuid);
//        const Transform& current = transformComponent.ObjectTransform;
//
//        const auto previousIt = m_PreviousTransforms.find(uuid);
//        const bool isDirty = previousIt == m_PreviousTransforms.end() || IsDifferent(previousIt->second, current);
//
//        if (TransformDirtyComponent* dirtyComponent = scene.TryGetComponent<TransformDirtyComponent>(handle)) {
//            dirtyComponent->Dirty = isDirty;
//        } else if (isDirty) {
//            if (Entity* entity = scene.TryGetEntity(handle))
//                entity->AddComponent<TransformDirtyComponent>(TransformDirtyComponent{ true });
//        }
//
//        m_PreviousTransforms[uuid] = current;
//    });
//
//    std::vector<UUID> toErase;
//    toErase.reserve(m_PreviousTransforms.size());
//    for (const auto& [uuid, transform] : m_PreviousTransforms) {
//        (void)transform;
//        if (seen.find(uuid) == seen.end())
//            toErase.push_back(uuid);
//    }
//    for (const UUID& uuid : toErase)
//        m_PreviousTransforms.erase(uuid);
//}
//
//bool TransformDirtySystem::IsDifferent(const Transform& left, const Transform& right)
//{
//    return !IsNearlyEqualVec3(left.Position, right.Position)
//        || !IsNearlyEqualVec3(left.Rotation, right.Rotation)
//        || !IsNearlyEqualVec3(left.Scale, right.Scale);
//}
//
//} // namespace CHEngine
