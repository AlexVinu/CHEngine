#pragma once

#include "CHEngine/World/ISystem.h"
#include "CHEngine/Scene/Scene.h"
#include <memory>
#include <cstdint>

namespace CHEngine {

// ─────────────────────────────────────────────────────────────────────────────
// Lua scripting system
//
// Состоит из ScriptHost (владеет sol::state, инстансами скриптов, хуками) и
// двух тонких ISystem-обёрток, разделяющих этот host:
//
//   LuaPreSimSystem  (priority 5, Simulation) — OnUpdate (до физики)
//   LuaPostSimSystem (priority 150, Simulation) — OnLateUpdate (после физики)
//
// PreSim также делает OnBegin/OnEnd: загружает/выгружает скрипты и подписывает
// хуки на добавление/удаление ScriptComponent.
//
// Уровни скриптов:
//   Entity scripts — ScriptComponent::Scripts[] на конкретной энтити
//   World scripts  — Scene::WorldScripts (глобальные для сцены)
//
// Lua-колбэки (все опциональны):
//   Entity:  OnStart(entity, world)
//            OnUpdate(entity, world, dt)
//            OnLateUpdate(entity, world, dt)
//            OnStop(entity, world)
//   World:   OnStart(world)
//            OnUpdate(world, dt)
//            OnLateUpdate(world, dt)
//            OnStop(world)
//
// Полный API — в LuaScriptSystem.cpp в SetupAPI().
// ─────────────────────────────────────────────────────────────────────────────

class ScriptHost;

class LuaPreSimSystem final : public ISystem
{
public:
    explicit LuaPreSimSystem(std::shared_ptr<ScriptHost> host, uint8_t priority = 5);
    ~LuaPreSimSystem() override;

    const char* GetName() const override { return "LuaPreSimSystem"; }

    void OnBegin(World& world, DeferredOps& deferred_ops) override;
    void Run(World& world, DeferredOps& deferred_ops, Timestep dt) override;
    void OnEnd(World& world, DeferredOps& deferred_ops) override;

private:
    std::shared_ptr<ScriptHost> m_Host;
};

class LuaPostSimSystem final : public ISystem
{
public:
    explicit LuaPostSimSystem(std::shared_ptr<ScriptHost> host, uint8_t priority = 150);
    ~LuaPostSimSystem() override;

    const char* GetName() const override { return "LuaPostSimSystem"; }

    void Run(World& world, DeferredOps& deferred_ops, Timestep dt) override;

private:
    std::shared_ptr<ScriptHost> m_Host;
};

// Factory: создаёт пару связанных систем с общим host'ом. Сами системы регистрируются
// вызывающей стороной (World::RegisterDefaultSystems).
std::shared_ptr<ScriptHost> MakeScriptHost();

} // namespace CHEngine
