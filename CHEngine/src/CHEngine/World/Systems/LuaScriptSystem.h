#pragma once

#include "CHEngine/World/ISystem.h"
#include "CHEngine/Scene/Scene.h"
#include <memory>
#include <unordered_map>
#include <cstdint>

namespace CHEngine {

// ─────────────────────────────────────────────────────────────────────────────
// LuaScriptSystem
//
// Фаза: Simulation (приоритет 5 — выполняется раньше физики).
//
// Жизненный цикл скрипта:
//   OnBegin  → загружает все ScriptComponent, вызывает OnStart(entity)
//   Run      → вызывает OnUpdate(entity, dt) каждый кадр
//   OnEnd    → вызывает OnStop(entity), очищает инстансы
//
// Lua API доступный в скриптах:
//   entity:GetName()                → string
//   entity:GetPosition()            → {x, y, z}
//   entity:GetRotation()            → {x, y, z} (Euler degrees)
//   entity:GetScale()               → {x, y, z}
//   entity:SetPosition(x, y, z)
//   entity:SetRotation(x, y, z)
//   entity:SetScale(x, y, z)
//   entity:GetColor()               → {r, g, b, a}
//   entity:SetColor(r, g, b, a)
//
//   Input.IsKeyDown(key)            → bool
//   Input.IsKeyPressed(key)         → bool
//   Input.IsKeyReleased(key)        → bool
//   Input.GetMouseX()               → float
//   Input.GetMouseY()               → float
//   Input.GetMouseDeltaX()          → float
//   Input.GetMouseDeltaY()          → float
//
//   Key.W, Key.A, Key.S, Key.D, Key.Space, Key.Escape ...
//
//   Log.Info(msg)
//   Log.Warn(msg)
//   Log.Error(msg)
// ─────────────────────────────────────────────────────────────────────────────

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
    // PIMPL — sol2 включается только в .cpp
    struct Impl;
    std::unique_ptr<Impl> m_Impl;

    static uint64_t MakeKey(EntityHandle h)
    {
        return (uint64_t)h.index | ((uint64_t)h.generation << 32u);
    }
};

} // namespace CHEngine
