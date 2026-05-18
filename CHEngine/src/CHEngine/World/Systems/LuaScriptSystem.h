#pragma once

#include "CHEngine/World/ISystem.h"
#include "CHEngine/Scene/Scene.h"
#include <memory>
#include <cstdint>

namespace CHEngine {

// ─────────────────────────────────────────────────────────────────────────────
// Lua scripting system
//
// Одна ISystem-обёртка, фаза Simulation (priority 5 — до физики).
// Внутри держит ScriptHost (sol::state, инстансы скриптов, хуки на
// добавление/удаление ScriptComponent).
//
// Уровни скриптов:
//   Entity scripts — ScriptComponent::Scripts[] на конкретной энтити
//   World scripts  — Scene::WorldScripts (глобальные для сцены)
//
// Lua-колбэки (все опциональны):
//   Entity:  OnStart(entity, world)
//            OnUpdate(entity, world, dt)
//            OnStop(entity, world)
//   World:   OnStart(world)
//            OnUpdate(world, dt)
//            OnStop(world)
//
// Полный API — в LuaScriptSystem.cpp в SetupAPI().
// ─────────────────────────────────────────────────────────────────────────────

class ScriptHost;

class LuaScriptSystem final : public ISystem
{
public:
    explicit LuaScriptSystem(uint8_t priority = 5);
    ~LuaScriptSystem() override;

    const char* GetName() const override { return "LuaScriptSystem"; }

    void OnBegin(World& world, DeferredOps& deferred_ops) override;
    void Run(World& world, DeferredOps& deferred_ops, Timestep dt) override;
    void OnEnd(World& world, DeferredOps& deferred_ops) override;

private:
    Ref<ScriptHost> m_Host;
};

// ─────────────────────────────────────────────────────────────────────────────
// Внешний API для вызова Lua-функций по имени из других C++ систем.
// Безопасный no-op, если: ScriptHost для данного World ещё/уже не зарегистрирован,
// у entity нет ScriptComponent, скрипт ещё не started, либо функция не определена.
// Аргументы: первым параметром в Lua-функцию всегда передаётся ScriptEntity,
// далее значения args (если есть).
// ─────────────────────────────────────────────────────────────────────────────
void LuaCallEntityFunction(World& world, DeferredOps& deferred,
                           EntityHandle entity, const char* fnName);
void LuaCallEntityFunction(World& world, DeferredOps& deferred,
                           EntityHandle entity, const char* fnName, float arg);

} // namespace CHEngine
