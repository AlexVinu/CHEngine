//#pragma once
//
//#include <unordered_map>
//
//#include <boost/container_hash/hash.hpp>
//
//#include "CHEngine/Scene/Transform.h"
//#include "CHEngine/World/ISystem.h"
//
//namespace CHEngine {
//
//class TransformDirtySystem final : public ISystem {
//public:
//    explicit TransformDirtySystem(uint8_t priority = 10)
//        : ISystem(SystemPhase::Simulation, priority) {}
//
//    const char* name() const override { return "TransformDirtySystem"; }
//    void run(Scene& scene, CommandBuffer& commands, Timestep dt) override;
//
//private:
//    static bool IsDifferent(const Transform& left, const Transform& right);
//
//private:
//    std::unordered_map<UUID, Transform, boost::hash<UUID>> m_PreviousTransforms;
//};
//
//} // namespace CHEngine
