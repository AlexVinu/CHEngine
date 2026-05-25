#include "chepch.h"
#include "LuaScriptSystem.h"

// sol2 — подключаем только здесь (PIMPL'е аналог: всё в .cpp).
// SOL_SAFE_REFERENCES даёт unprotected lua_error() на LUA_REGISTRYINDEX
// (см. историю проекта) — оставляем выключенным.
#define SOL_SAFE_USERTYPE   1
#define SOL_SAFE_FUNCTION   1
#define SOL_SAFE_GETTER     1
#define SOL_SAFE_NUMERICS   0
#define SOL_SAFE_REFERENCES 0
#include <sol/sol.hpp>

#include "CHEngine/World/World.h"
#include "CHEngine/World/DeferredOps.h"
#include "CHEngine/Scene/Scene.h"
#include "CHEngine/Scene/Entity.h"
#include "CHEngine/Scene/Components.h"
#include "CHEngine/Input/InputSystem.h"
#include "CHEngine/Application.h"
#include <Input/KeyCodes.h>
#include "Log/Log.h"

#include <filesystem>
#include <unordered_map>
#include <string>
#include <mutex>

namespace CHEngine {

// ═════════════════════════════════════════════════════════════════════════════
// Forward
// ═════════════════════════════════════════════════════════════════════════════
class ScriptHost;

// ─── Глобальный диспатч World* → ScriptHost* для функций-указателей хуков ────
// Component hooks в DeferredOps — это `void(*)(World&, EntityHandle)`,
// без захвата. Дойти до нужного host'а можно только через World, поэтому
// держим небольшую таблицу. Доступ — из главного потока (как и весь scheduler).
static std::unordered_map<World*, ScriptHost*> g_HostByWorld;
static std::mutex g_HostMapMutex;

static void RegisterHost(World* w, ScriptHost* h)
{
    std::lock_guard lk(g_HostMapMutex);
    g_HostByWorld[w] = h;
}
static void UnregisterHost(World* w)
{
    std::lock_guard lk(g_HostMapMutex);
    g_HostByWorld.erase(w);
}
static ScriptHost* FindHost(World* w)
{
    std::lock_guard lk(g_HostMapMutex);
    auto it = g_HostByWorld.find(w);
    return it == g_HostByWorld.end() ? nullptr : it->second;
}

// ─── Lua panic handler (без него unprotected lua_error → abort) ──────────────
static int LuaPanicHandler(lua_State* L)
{
    const char* msg = lua_tostring(L, -1);
    CHE_CORE_CRITICAL("[Lua] PANIC: {}", msg ? msg : "(unknown error)");
    throw std::runtime_error(msg ? msg : "Lua panic");
}

// ═════════════════════════════════════════════════════════════════════════════
// ScriptEntity — обёртка вокруг EntityHandle для Lua
// ═════════════════════════════════════════════════════════════════════════════
struct ScriptEntity
{
    EntityHandle handle;
    World*       world    = nullptr;
    DeferredOps* deferred = nullptr;

    Scene* GetScene() const { return world ? world->GetSceneRef().get() : nullptr; }
    Entity* GetEntity() const
    {
        auto* scene = GetScene();
        return scene ? scene->TryGetEntity(handle) : nullptr;
    }

    bool IsValid() const { return GetEntity() != nullptr; }

    std::string GetName() const
    {
        auto* e = GetEntity();
        if (!e || !e->HasComponent<TagComponent>()) return "Unknown";
        auto* sc = e->GetScene();
        return sc ? sc->GetString(e->GetComponent<TagComponent>().Name) : "Unknown";
    }

    std::string GetUUID() const
    {
        auto* scene = GetScene();
        if (!scene) return {};
        return scene->GetUUID(handle).ToString();
    }

    void Destroy() const
    {
        if (deferred && handle.IsValid())
            deferred->DestroyEntity(handle);
    }

    void TransferTo(const std::string& worldName) const
    {
        if (deferred && handle.IsValid())
            deferred->TransferEntity(handle, worldName);
    }

    // ── HasX ─────────────────────────────────────────────────────────────────
    template<typename T>
    bool HasT() const
    {
        auto* e = GetEntity();
        return e && e->HasComponent<T>();
    }
    bool HasTransform()  const { return HasT<TransformComponent>(); }
    bool HasColor()      const { return HasT<ColorComponent>();     }
    bool HasVisibility() const { return HasT<VisibilityComponent>(); }
    bool HasLifetime()   const { return HasT<LifetimeComponent>();  }
    bool HasLight()      const { return HasT<LightComponent>();     }
    bool HasCamera()     const { return HasT<CameraComponent>();    }
    bool HasMesh()       const { return HasT<MeshComponent>();      }
    bool HasRigidBody()  const { return HasT<RigidBody3DComponent>(); }
    bool HasScript()     const { return HasT<ScriptComponent>();    }

    // ── AddX (deferred) ──────────────────────────────────────────────────────
    void AddTransform()                            const { if (deferred) deferred->AddComponent<TransformComponent>(handle); }
    void AddColor(float r, float g, float b, float a) const
    {
        if (!deferred) return;
        deferred->AddComponent<ColorComponent>(handle, ColorComponent{ { r, g, b, a } });
    }
    void AddVisibility(bool visible) const
    {
        if (!deferred) return;
        deferred->AddComponent<VisibilityComponent>(handle, VisibilityComponent{ visible });
    }
    void AddLifetime(float seconds, bool destroyOnExpire) const
    {
        if (!deferred) return;
        deferred->AddComponent<LifetimeComponent>(handle, LifetimeComponent{ seconds, destroyOnExpire });
    }
    void AddLight(const std::string& typeStr, float intensity,
                  float r, float g, float b, float range) const
    {
        if (!deferred) return;
        Light data;
        if (typeStr == "Directional")      data.Type = LightType::Directional;
        else if (typeStr == "Point")       data.Type = LightType::Point;
        else if (typeStr == "Spot")        data.Type = LightType::Spot;
        else
        {
            CHE_CORE_WARN("[Lua] AddLight: unknown type '{}', defaulting to Point", typeStr);
            data.Type = LightType::Point;
        }
        data.Color     = { r, g, b };
        data.Intensity = intensity;
        data.Range     = range;
        deferred->AddComponent<LightComponent>(handle, LightComponent{ data });
    }

    // ── RemoveX (deferred) ───────────────────────────────────────────────────
    void RemoveTransform()  const { if (deferred) deferred->RemoveComponent<TransformComponent>(handle); }
    void RemoveColor()      const { if (deferred) deferred->RemoveComponent<ColorComponent>(handle); }
    void RemoveVisibility() const { if (deferred) deferred->RemoveComponent<VisibilityComponent>(handle); }
    void RemoveLifetime()   const { if (deferred) deferred->RemoveComponent<LifetimeComponent>(handle); }
    void RemoveLight()      const { if (deferred) deferred->RemoveComponent<LightComponent>(handle); }

    // ── Get/Set (немедленно) ─────────────────────────────────────────────────
    static sol::table Vec3Table(sol::state_view lua, float x, float y, float z)
    {
        auto t = lua.create_table();
        t["x"] = x; t["y"] = y; t["z"] = z;
        return t;
    }

    sol::table GetPosition(sol::this_state s) const
    {
        sol::state_view lua(s);
        if (auto* e = GetEntity(); e && e->HasComponent<TransformComponent>())
        {
            const auto& p = e->GetComponent<TransformComponent>().ObjectTransform.Position;
            return Vec3Table(lua, p.x, p.y, p.z);
        }
        return Vec3Table(lua, 0, 0, 0);
    }
    void SetPosition(float x, float y, float z) const
    {
        if (auto* e = GetEntity(); e && e->HasComponent<TransformComponent>())
        {
            auto& tc = e->GetComponent<TransformComponent>();
            tc.ObjectTransform.Position = { x, y, z };
            tc.MarkDirty();
        }
    }

    sol::table GetRotation(sol::this_state s) const
    {
        sol::state_view lua(s);
        if (auto* e = GetEntity(); e && e->HasComponent<TransformComponent>())
        {
            const auto& r = e->GetComponent<TransformComponent>().ObjectTransform.Rotation;
            return Vec3Table(lua, r.x, r.y, r.z);
        }
        return Vec3Table(lua, 0, 0, 0);
    }
    void SetRotation(float x, float y, float z) const
    {
        if (auto* e = GetEntity(); e && e->HasComponent<TransformComponent>())
        {
            auto& tc = e->GetComponent<TransformComponent>();
            tc.ObjectTransform.Rotation = { x, y, z };
            tc.MarkDirty();
        }
    }

    sol::table GetScale(sol::this_state s) const
    {
        sol::state_view lua(s);
        if (auto* e = GetEntity(); e && e->HasComponent<TransformComponent>())
        {
            const auto& sc = e->GetComponent<TransformComponent>().ObjectTransform.Scale;
            return Vec3Table(lua, sc.x, sc.y, sc.z);
        }
        return Vec3Table(lua, 1, 1, 1);
    }
    void SetScale(float x, float y, float z) const
    {
        if (auto* e = GetEntity(); e && e->HasComponent<TransformComponent>())
        {
            auto& tc = e->GetComponent<TransformComponent>();
            tc.ObjectTransform.Scale = { x, y, z };
            tc.MarkDirty();
        }
    }

    sol::table GetColor(sol::this_state s) const
    {
        sol::state_view lua(s);
        auto t = lua.create_table();
        if (auto* e = GetEntity(); e && e->HasComponent<ColorComponent>())
        {
            const auto& c = e->GetComponent<ColorComponent>().Color;
            t["r"] = c.r; t["g"] = c.g; t["b"] = c.b; t["a"] = c.a;
        }
        else
        {
            t["r"] = 1.0f; t["g"] = 1.0f; t["b"] = 1.0f; t["a"] = 1.0f;
        }
        return t;
    }
    void SetColor(float r, float g, float b, float a) const
    {
        auto* e = GetEntity();
        if (!e) return;
        if (!e->HasComponent<ColorComponent>())
            e->AddComponent<ColorComponent>();
        e->GetComponent<ColorComponent>().Color = { r, g, b, a };
    }

    bool GetVisibility() const
    {
        if (auto* e = GetEntity(); e && e->HasComponent<VisibilityComponent>())
            return e->GetComponent<VisibilityComponent>().Visible;
        return false;
    }
    void SetVisibility(bool v) const
    {
        if (auto* e = GetEntity(); e && e->HasComponent<VisibilityComponent>())
            e->GetComponent<VisibilityComponent>().Visible = v;
    }

    float GetLifetime() const
    {
        if (auto* e = GetEntity(); e && e->HasComponent<LifetimeComponent>())
            return e->GetComponent<LifetimeComponent>().RemainingSeconds;
        return 0.0f;
    }
    void SetLifetime(float seconds) const
    {
        if (auto* e = GetEntity(); e && e->HasComponent<LifetimeComponent>())
            e->GetComponent<LifetimeComponent>().RemainingSeconds = seconds;
    }

    float GetLightIntensity() const
    {
        if (auto* e = GetEntity(); e && e->HasComponent<LightComponent>())
            return e->GetComponent<LightComponent>().LightData.Intensity;
        return 0.0f;
    }
    void SetLightIntensity(float v) const
    {
        if (auto* e = GetEntity(); e && e->HasComponent<LightComponent>())
            e->GetComponent<LightComponent>().LightData.Intensity = v;
    }

    sol::table GetLightColor(sol::this_state s) const
    {
        sol::state_view lua(s);
        if (auto* e = GetEntity(); e && e->HasComponent<LightComponent>())
        {
            const auto& c = e->GetComponent<LightComponent>().LightData.Color;
            return Vec3Table(lua, c.r, c.g, c.b);
        }
        return Vec3Table(lua, 1, 1, 1);
    }
    void SetLightColor(float r, float g, float b) const
    {
        if (auto* e = GetEntity(); e && e->HasComponent<LightComponent>())
            e->GetComponent<LightComponent>().LightData.Color = { r, g, b };
    }

    float GetLightRange() const
    {
        if (auto* e = GetEntity(); e && e->HasComponent<LightComponent>())
            return e->GetComponent<LightComponent>().LightData.Range;
        return 0.0f;
    }
    void SetLightRange(float v) const
    {
        if (auto* e = GetEntity(); e && e->HasComponent<LightComponent>())
            e->GetComponent<LightComponent>().LightData.Range = v;
    }
};

// ═════════════════════════════════════════════════════════════════════════════
// ScriptWorld — обёртка вокруг World для Lua
// ═════════════════════════════════════════════════════════════════════════════
struct ScriptWorld
{
    World*       world    = nullptr;
    DeferredOps* deferred = nullptr;

    ScriptEntity MakeEntity(EntityHandle h) const { return ScriptEntity{ h, world, deferred }; }

    sol::object FindByName(const std::string& name, sol::this_state s) const
    {
        sol::state_view lua(s);
        auto* scene = world ? world->GetSceneRef().get() : nullptr;
        if (!scene) return sol::lua_nil;

        EntityHandle found{};
        scene->ForEach<TagComponent>([&](EntityHandle h, const UUID&, TagComponent& tag) {
            if (!found.IsValid() && scene->GetString(tag.Name) == name)
                found = h;
        });
        if (!found.IsValid()) return sol::lua_nil;
        return sol::make_object(lua, MakeEntity(found));
    }

    sol::object FindByUUID(const std::string& uuidStr, sol::this_state s) const
    {
        sol::state_view lua(s);
        auto* scene = world ? world->GetSceneRef().get() : nullptr;
        if (!scene) return sol::lua_nil;

        UUID uuid = UUID::FromString(uuidStr);
        if (!uuid.IsValid()) return sol::lua_nil;

        EntityHandle h = scene->TryGetEntityHandleByUUID(uuid);
        if (!h.IsValid()) return sol::lua_nil;
        return sol::make_object(lua, MakeEntity(h));
    }

    std::string SpawnEntity(const std::string& name) const
    {
        if (!deferred) return {};
        UUID uuid = UUID::Generate();
        deferred->CreateEntityWithUUID(name, uuid);
        return uuid.ToString();
    }

    void DestroyEntity(const ScriptEntity& e) const
    {
        if (deferred && e.handle.IsValid())
            deferred->DestroyEntity(e.handle);
    }

    std::string GetName() const { return world ? world->GetWorldName() : ""; }

    // Override the automatic world-state logic after a camera transfer.
    void SetState(const std::string& stateStr) const
    {
        if (!world) return;
        if (stateStr == "Simulating")
            world->SetState(WorldState::Simulating);
        else if (stateStr == "SimulatingWithoutPresenting")
            world->SetState(WorldState::SimulatingWithoutPresenting);
        else if (stateStr == "Presenting")
            world->SetState(WorldState::Presenting);
        else
            CHE_CORE_WARN("[Lua] ScriptWorld:SetState — unknown state '{}'", stateStr);
    }

    void ForEach(sol::function fn, sol::this_state s) const
    {
        sol::state_view lua(s);
        auto* scene = world ? world->GetSceneRef().get() : nullptr;
        if (!scene) return;
        scene->ForEach<IDComponent>([&](EntityHandle h, const UUID&, IDComponent&) {
            ScriptEntity se = MakeEntity(h);
            auto res = fn(se);
            if (!res.valid())
            {
                sol::error err = res;
                CHE_CORE_ERROR("[Lua] ForEach callback error: {}", err.what());
            }
        });
    }
};

// ═════════════════════════════════════════════════════════════════════════════
// ScriptKey / ScriptInstance
// ═════════════════════════════════════════════════════════════════════════════
struct ScriptKey
{
    enum class Kind : uint8_t { Entity, World };
    Kind     kind;
    uint64_t entityKey;   // 0 для World scripts
    uint32_t scriptIndex; // индекс в ScriptComponent::Scripts или Scene::WorldScripts

    bool operator==(const ScriptKey& o) const noexcept
    {
        return kind == o.kind && entityKey == o.entityKey && scriptIndex == o.scriptIndex;
    }
};

struct ScriptKeyHash
{
    size_t operator()(const ScriptKey& k) const noexcept
    {
        size_t h = std::hash<uint64_t>{}(k.entityKey);
        h ^= std::hash<uint32_t>{}(k.scriptIndex) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= (size_t)k.kind + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

struct ScriptInstance
{
    sol::environment   env;
    sol::safe_function onStart;
    sol::safe_function onUpdate;
    sol::safe_function onStop;
    bool               started  = false;
    bool               hasError = false;  // после первой ошибки в Update — не вызывать снова
    std::string        path;
};

static uint64_t EntityKey(EntityHandle h)
{
    return (uint64_t)h.index | ((uint64_t)h.generation << 32u);
}

// ═════════════════════════════════════════════════════════════════════════════
// ScriptHost
// ═════════════════════════════════════════════════════════════════════════════
class ScriptHost
{
public:
    ScriptHost()
    {
        lua_atpanic(m_Lua.lua_state(), &LuaPanicHandler);
        SetupAPI();
    }

    // ── OnBegin (PreSim) ─────────────────────────────────────────────────────
    void OnBegin(World& world, DeferredOps& deferred)
    {
        m_Instances.clear();
        RegisterHost(&world, this);

        // Подписки на дин. загрузку/выгрузку ScriptComponent
        m_AddedHook = deferred.SubscribeOnComponentAdded<ScriptComponent>(&OnScriptComponentAddedTrampoline);
        m_RemovedHook = deferred.SubscribeOnComponentRemoved<ScriptComponent>(&OnScriptComponentRemovedTrampoline);

        auto scene = world.GetSceneRef();
        if (!scene) return;

        // Entity scripts
        scene->ForEach<ScriptComponent>([&](EntityHandle handle, const UUID&, ScriptComponent& sc) {
            LoadEntityScripts(world, deferred, handle, sc);
        });

        // World scripts
        LoadWorldScripts(world, deferred);

        CHE_CORE_INFO("[Lua] Loaded {} script instance(s)", m_Instances.size());
    }

    // ── OnEnd (PreSim) ───────────────────────────────────────────────────────
    void OnEnd(World& world, DeferredOps& deferred)
    {
        // OnStop для всех инстансов
        auto scene = world.GetSceneRef();
        for (auto& [key, inst] : m_Instances)
        {
            if (!inst.started || !inst.onStop.valid()) continue;
            try
            {
                if (key.kind == ScriptKey::Kind::Entity && scene)
                {
                    EntityHandle h{};
                    h.index = (uint32_t)(key.entityKey & 0xFFFFFFFFu);
                    h.generation = (uint32_t)((key.entityKey >> 32) & 0xFFFFFFFFu);
                    ScriptEntity se{ h, &world, &deferred };
                    ScriptWorld sw{ &world, &deferred };
                    auto res = inst.onStop(se, sw);
                    if (!res.valid()) { sol::error e = res; CHE_CORE_ERROR("[Lua] OnStop '{}': {}", inst.path, e.what()); }
                }
                else
                {
                    ScriptWorld sw{ &world, &deferred };
                    auto res = inst.onStop(sw);
                    if (!res.valid()) { sol::error e = res; CHE_CORE_ERROR("[Lua] OnStop '{}': {}", inst.path, e.what()); }
                }
            }
            catch (const std::exception& ex)
            {
                CHE_CORE_ERROR("[Lua] Exception in OnStop '{}': {}", inst.path, ex.what());
            }
        }

        if (m_AddedHook.IsValid())   deferred.Unsubscribe(m_AddedHook);
        if (m_RemovedHook.IsValid()) deferred.Unsubscribe(m_RemovedHook);
        m_AddedHook = {};
        m_RemovedHook = {};

        m_Instances.clear();
        UnregisterHost(&world);
        CHE_CORE_INFO("[Lua] Scripts stopped and cleared");
    }

    // ── Run ──────────────────────────────────────────────────────────────────
    void Run(World& world, DeferredOps& deferred, Timestep dt)
    {
        DispatchUpdate(world, deferred, dt);
    }

    // ── Вызов произвольной функции по имени на всех скриптах сущности ────────
    template<typename... Args>
    void CallEntityFunctionImpl(World& world, DeferredOps& deferred,
                                EntityHandle handle, const char* fnName, Args&&... args)
    {
        auto scene = world.GetSceneRef();
        if (!scene || !scene->IsEntityHandleValid(handle)) return;
        const uint64_t ek = EntityKey(handle);
        ScriptEntity se{ handle, &world, &deferred };
        for (auto& [key, inst] : m_Instances)
        {
            if (key.kind != ScriptKey::Kind::Entity) continue;
            if (key.entityKey != ek) continue;
            if (!inst.started || inst.hasError) continue;
            sol::object obj = inst.env.get<sol::object>(fnName);
            if (!obj.is<sol::safe_function>()) continue;
            sol::safe_function fn = obj.as<sol::safe_function>();
            try
            {
                auto res = fn(se, std::forward<Args>(args)...);
                if (!res.valid())
                {
                    sol::error e = res;
                    CHE_CORE_ERROR("[Lua] {} '{}': {}", fnName, inst.path, e.what());
                }
            }
            catch (const std::exception& ex)
            {
                CHE_CORE_ERROR("[Lua] Exception in {} '{}': {}", fnName, inst.path, ex.what());
            }
        }
    }

    void CallEntityFunction(World& world, DeferredOps& deferred,
                            EntityHandle handle, const char* fnName)
    { CallEntityFunctionImpl(world, deferred, handle, fnName); }

    void CallEntityFunction(World& world, DeferredOps& deferred,
                            EntityHandle handle, const char* fnName, float arg)
    { CallEntityFunctionImpl(world, deferred, handle, fnName, arg); }

private:
    sol::state                                                            m_Lua;
    std::unordered_map<ScriptKey, ScriptInstance, ScriptKeyHash>          m_Instances;
    DeferredOps::HookHandle                                               m_AddedHook;
    DeferredOps::HookHandle                                               m_RemovedHook;

    // ── Хук-trampolines (function-pointer типов в DeferredOps) ───────────────
    static void OnScriptComponentAddedTrampoline(World& w, EntityHandle h)
    {
        ScriptHost* host = FindHost(&w);
        if (!host) return;
        auto scene = w.GetSceneRef();
        if (!scene) return;
        auto* entity = scene->TryGetEntity(h);
        if (!entity || !entity->HasComponent<ScriptComponent>()) return;
        auto& sc = entity->GetComponent<ScriptComponent>();
        // DeferredOps мы тут не имеем — для динамических операций из скриптов
        // нам нужен deferred. Берём его из World.
        host->LoadEntityScripts(w, w.GetDeferredOps(), h, sc);
    }

    static void OnScriptComponentRemovedTrampoline(World& w, EntityHandle h)
    {
        ScriptHost* host = FindHost(&w);
        if (!host) return;
        host->StopAndUnloadEntity(w, w.GetDeferredOps(), h);
    }

    // ── Загрузка скриптов энтити ─────────────────────────────────────────────
    void LoadEntityScripts(World& world, DeferredOps& deferred, EntityHandle handle, const ScriptComponent& sc)
    {
        auto* scene   = world.GetSceneRef().get();
        const auto* entries = scene ? scene->GetScripts(sc.Scripts) : nullptr;
        if (!entries) return;

        for (uint32_t i = 0; i < (uint32_t)entries->size(); ++i)
        {
            const auto& entry = (*entries)[i];
            if (!entry.Enabled || entry.Path.empty()) continue;

            ScriptKey key{ ScriptKey::Kind::Entity, EntityKey(handle), i };
            if (m_Instances.find(key) != m_Instances.end())
                continue; // уже загружен

            try
            {
                ScriptInstance inst;
                if (!LoadFile(entry.Path, inst)) continue;
                m_Instances[key] = std::move(inst);

                auto& live = m_Instances[key];
                if (live.onStart.valid())
                {
                    ScriptEntity se{ handle, &world, &deferred };
                    ScriptWorld  sw{ &world, &deferred };
                    auto res = live.onStart(se, sw);
                    if (!res.valid())
                    {
                        sol::error e = res;
                        CHE_CORE_ERROR("[Lua] OnStart '{}': {}", live.path, e.what());
                        live.hasError = true;
                    }
                }
                live.started = true;
            }
            catch (const std::exception& ex)
            {
                CHE_CORE_ERROR("[Lua] Exception loading '{}': {}", entry.Path, ex.what());
            }
        }
    }

    void LoadWorldScripts(World& world, DeferredOps& deferred)
    {
        auto scene = world.GetSceneRef();
        if (!scene) return;
        for (uint32_t i = 0; i < (uint32_t)scene->WorldScripts.size(); ++i)
        {
            const auto& entry = scene->WorldScripts[i];
            if (!entry.Enabled || entry.Path.empty()) continue;

            ScriptKey key{ ScriptKey::Kind::World, 0, i };
            try
            {
                ScriptInstance inst;
                if (!LoadFile(entry.Path, inst)) continue;
                m_Instances[key] = std::move(inst);

                auto& live = m_Instances[key];
                if (live.onStart.valid())
                {
                    ScriptWorld sw{ &world, &deferred };
                    auto res = live.onStart(sw);
                    if (!res.valid())
                    {
                        sol::error e = res;
                        CHE_CORE_ERROR("[Lua] OnStart '{}': {}", live.path, e.what());
                        live.hasError = true;
                    }
                }
                live.started = true;
            }
            catch (const std::exception& ex)
            {
                CHE_CORE_ERROR("[Lua] Exception loading world script '{}': {}", entry.Path, ex.what());
            }
        }
    }

    void StopAndUnloadEntity(World& world, DeferredOps& deferred, EntityHandle handle)
    {
        const uint64_t ek = EntityKey(handle);
        for (auto it = m_Instances.begin(); it != m_Instances.end(); )
        {
            if (it->first.kind == ScriptKey::Kind::Entity && it->first.entityKey == ek)
            {
                if (it->second.started && it->second.onStop.valid())
                {
                    try
                    {
                        ScriptEntity se{ handle, &world, &deferred };
                        ScriptWorld  sw{ &world, &deferred };
                        auto res = it->second.onStop(se, sw);
                        if (!res.valid()) { sol::error e = res; CHE_CORE_ERROR("[Lua] OnStop '{}': {}", it->second.path, e.what()); }
                    }
                    catch (const std::exception& ex)
                    {
                        CHE_CORE_ERROR("[Lua] Exception in OnStop '{}': {}", it->second.path, ex.what());
                    }
                }
                it = m_Instances.erase(it);
            }
            else ++it;
        }
    }

    // ── Загрузка одного файла в инстанс ──────────────────────────────────────
    bool LoadFile(const std::string& path, ScriptInstance& outInst)
    {
        if (!std::filesystem::exists(path))
        {
            CHE_CORE_ERROR("[Lua] Script not found: {}", path);
            return false;
        }

        sol::environment env(m_Lua, sol::create, m_Lua.globals());
        auto result = m_Lua.safe_script_file(path, env, sol::script_pass_on_error);
        if (!result.valid())
        {
            sol::error err = result;
            CHE_CORE_ERROR("[Lua] Error loading '{}': {}", path, err.what());
            return false;
        }

        outInst.env  = std::move(env);
        outInst.path = path;

        auto pick = [&](const char* name) -> sol::safe_function {
            auto obj = outInst.env.get<sol::object>(name);
            if (obj.is<sol::safe_function>()) return obj.as<sol::safe_function>();
            return {};
        };
        outInst.onStart  = pick("OnStart");
        outInst.onUpdate = pick("OnUpdate");
        outInst.onStop   = pick("OnStop");
        return true;
    }

    // ── Диспатч OnUpdate ─────────────────────────────────────────────────────
    void DispatchUpdate(World& world, DeferredOps& deferred, Timestep dt)
    {
        auto scene = world.GetSceneRef();
        if (!scene) return;

        for (auto& [key, inst] : m_Instances)
        {
            if (!inst.started)  continue;
            if (inst.hasError)  continue;  
            if (!inst.onUpdate.valid()) continue;

            try
            {
                if (key.kind == ScriptKey::Kind::Entity)
                {
                    EntityHandle h{};
                    h.index      = (uint32_t)(key.entityKey & 0xFFFFFFFFu);
                    h.generation = (uint32_t)((key.entityKey >> 32) & 0xFFFFFFFFu);
                    if (!scene->IsEntityHandleValid(h)) continue;
                    ScriptEntity se{ h, &world, &deferred };
                    ScriptWorld  sw{ &world, &deferred };
                    auto res = inst.onUpdate(se, sw, (float)dt);
                    if (!res.valid())
                    {
                        sol::error e = res;
                        CHE_CORE_ERROR("[Lua] OnUpdate '{}': {}", inst.path, e.what());
                        CHE_CORE_ERROR("[Lua] Script '{}' disabled until Play is restarted", inst.path);
                        inst.hasError = true;
                    }
                }
                else
                {
                    ScriptWorld sw{ &world, &deferred };
                    auto res = inst.onUpdate(sw, (float)dt);
                    if (!res.valid())
                    {
                        sol::error e = res;
                        CHE_CORE_ERROR("[Lua] OnUpdate '{}': {}", inst.path, e.what());
                        CHE_CORE_ERROR("[Lua] Script '{}' disabled until Play is restarted", inst.path);
                        inst.hasError = true;
                    }
                }
            }
            catch (const std::exception& ex)
            {
                CHE_CORE_ERROR("[Lua] Exception in OnUpdate '{}': {}", inst.path, ex.what());
                inst.hasError = true;
            }
        }
    }

    // ── Lua API регистрация ──────────────────────────────────────────────────
    void SetupAPI()
    {
        m_Lua.open_libraries(
            sol::lib::base, sol::lib::math, sol::lib::string,
            sol::lib::table, sol::lib::io,  sol::lib::os
        );

        // ── Entity ────────────────────────────────────────────────────────────
        m_Lua.new_usertype<ScriptEntity>("Entity",
            "GetName",        &ScriptEntity::GetName,
            "GetUUID",        &ScriptEntity::GetUUID,
            "IsValid",        &ScriptEntity::IsValid,
            "Destroy",        &ScriptEntity::Destroy,
            "TransferTo",     &ScriptEntity::TransferTo,

            "HasTransform",   &ScriptEntity::HasTransform,
            "HasColor",       &ScriptEntity::HasColor,
            "HasVisibility",  &ScriptEntity::HasVisibility,
            "HasLifetime",    &ScriptEntity::HasLifetime,
            "HasLight",       &ScriptEntity::HasLight,
            "HasCamera",      &ScriptEntity::HasCamera,
            "HasMesh",        &ScriptEntity::HasMesh,
            "HasRigidBody",   &ScriptEntity::HasRigidBody,
            "HasScript",      &ScriptEntity::HasScript,

            "AddTransform",   &ScriptEntity::AddTransform,
            "AddColor",       &ScriptEntity::AddColor,
            "AddVisibility",  &ScriptEntity::AddVisibility,
            "AddLifetime",    &ScriptEntity::AddLifetime,
            "AddLight",       &ScriptEntity::AddLight,

            "RemoveTransform",  &ScriptEntity::RemoveTransform,
            "RemoveColor",      &ScriptEntity::RemoveColor,
            "RemoveVisibility", &ScriptEntity::RemoveVisibility,
            "RemoveLifetime",   &ScriptEntity::RemoveLifetime,
            "RemoveLight",      &ScriptEntity::RemoveLight,

            "GetPosition",    &ScriptEntity::GetPosition,
            "SetPosition",    &ScriptEntity::SetPosition,
            "GetRotation",    &ScriptEntity::GetRotation,
            "SetRotation",    &ScriptEntity::SetRotation,
            "GetScale",       &ScriptEntity::GetScale,
            "SetScale",       &ScriptEntity::SetScale,

            "GetColor",       &ScriptEntity::GetColor,
            "SetColor",       &ScriptEntity::SetColor,

            "GetVisibility",  &ScriptEntity::GetVisibility,
            "SetVisibility",  &ScriptEntity::SetVisibility,

            "GetLifetime",    &ScriptEntity::GetLifetime,
            "SetLifetime",    &ScriptEntity::SetLifetime,

            "GetLightIntensity", &ScriptEntity::GetLightIntensity,
            "SetLightIntensity", &ScriptEntity::SetLightIntensity,
            "GetLightColor",     &ScriptEntity::GetLightColor,
            "SetLightColor",     &ScriptEntity::SetLightColor,
            "GetLightRange",     &ScriptEntity::GetLightRange,
            "SetLightRange",     &ScriptEntity::SetLightRange
        );

        // ── World ─────────────────────────────────────────────────────────────
        m_Lua.new_usertype<ScriptWorld>("World",
            "GetName",       &ScriptWorld::GetName,
            "SetState",      &ScriptWorld::SetState,
            "FindByName",    &ScriptWorld::FindByName,
            "FindByUUID",    &ScriptWorld::FindByUUID,
            "SpawnEntity",   &ScriptWorld::SpawnEntity,
            "DestroyEntity", &ScriptWorld::DestroyEntity,
            "ForEach",       &ScriptWorld::ForEach
        );

        // ── Actions (InputSystem) ─────────────────────────────────────────────
        if (auto* is = Application::Get().InputSystem()) {
            auto actions = m_Lua.create_named_table("Actions");
            actions["Triggered"]      = [is](const std::string& a) { return is->Triggered(a); };
            actions["Down"]           = [is](const std::string& a) { return is->Down(a); };
            actions["Released"]       = [is](const std::string& a) { return is->Released(a); };
            actions["GetAxis"]        = [is](int a) { return is->GetAxis(static_cast<InputSystem::Axis>(a)); };
            actions["PushContext"]    = [is](const std::string& c) { is->PushContext(c); };
            actions["PopContext"]     = [is](const std::string& c) { is->PopContext(c); };
            actions["IsActiveContext"]= [is](const std::string& c) { return is->IsActiveContext(c); };

            auto ax = m_Lua.create_named_table("Axis");
            ax["MouseDeltaX"] = (int)InputSystem::Axis::MouseDeltaX;
            ax["MouseDeltaY"] = (int)InputSystem::Axis::MouseDeltaY;
            ax["MouseWheel"]  = (int)InputSystem::Axis::MouseWheel;
            actions["Axis"] = ax;
        }

        // ── Key (CHEngine Key::* codes) ───────────────────────────────────────
        auto K = m_Lua.create_named_table("Key");
        K["Space"]     = (int)Key::Space;
        K["A"]=(int)Key::A; K["B"]=(int)Key::B; K["C"]=(int)Key::C; K["D"]=(int)Key::D;
        K["E"]=(int)Key::E; K["F"]=(int)Key::F; K["G"]=(int)Key::G; K["H"]=(int)Key::H;
        K["I"]=(int)Key::I; K["J"]=(int)Key::J; K["K"]=(int)Key::K; K["L"]=(int)Key::L;
        K["M"]=(int)Key::M; K["N"]=(int)Key::N; K["O"]=(int)Key::O; K["P"]=(int)Key::P;
        K["Q"]=(int)Key::Q; K["R"]=(int)Key::R; K["S"]=(int)Key::S; K["T"]=(int)Key::T;
        K["U"]=(int)Key::U; K["V"]=(int)Key::V; K["W"]=(int)Key::W; K["X"]=(int)Key::X;
        K["Y"]=(int)Key::Y; K["Z"]=(int)Key::Z;
        K["Escape"]    = (int)Key::Escape;      K["Enter"]     = (int)Key::Enter;
        K["Tab"]       = (int)Key::Tab;         K["Backspace"] = (int)Key::Backspace;
        K["Left"]      = (int)Key::Left;        K["Right"]     = (int)Key::Right;
        K["Up"]        = (int)Key::Up;          K["Down"]      = (int)Key::Down;
        K["LeftShift"] = (int)Key::LeftShift;
        K["LeftCtrl"]  = (int)Key::LeftControl;
        K["F1"]=(int)Key::F1; K["F2"]=(int)Key::F2; K["F3"]=(int)Key::F3;
        K["F4"]=(int)Key::F4; K["F5"]=(int)Key::F5; K["F12"]=(int)Key::F12;

        // ── Input (raw key polling via InputSystem, no action bindings) ───────
        if (auto* is = Application::Get().InputSystem()) {
            auto inp = m_Lua.create_named_table("Input");
            inp["IsKeyDown"]     = [is](int key) { return is->IsKeyDown(key);     };
            inp["IsKeyPressed"]  = [is](int key) { return is->IsKeyPressed(key);  };
            inp["IsKeyReleased"] = [is](int key) { return is->IsKeyReleased(key); };
        }

        // ── Log ──────────────────────────────────────────────────────────────
        auto log = m_Lua.create_named_table("Log");
        log["Info"]  = [](const std::string& m) { CHE_CORE_INFO("[Lua] {}", m); };
        log["Warn"]  = [](const std::string& m) { CHE_CORE_WARN("[Lua] {}", m); };
        log["Error"] = [](const std::string& m) { CHE_CORE_ERROR("[Lua] {}", m); };
    }
};

// ═════════════════════════════════════════════════════════════════════════════
// LuaScriptSystem — единственная ISystem-обёртка
// ═════════════════════════════════════════════════════════════════════════════
LuaScriptSystem::LuaScriptSystem(uint8_t priority)
    : ISystem(SystemPhase::Simulation, priority)
    , m_Host(std::make_shared<ScriptHost>())
{}

LuaScriptSystem::~LuaScriptSystem() = default;

void LuaScriptSystem::OnBegin(World& world, DeferredOps& deferred_ops)
{
    if (m_Host) m_Host->OnBegin(world, deferred_ops);
}

void LuaScriptSystem::Run(World& world, DeferredOps& deferred_ops, Timestep dt)
{
    if (m_Host) m_Host->Run(world, deferred_ops, dt);
}

void LuaScriptSystem::OnEnd(World& world, DeferredOps& deferred_ops)
{
    if (m_Host) m_Host->OnEnd(world, deferred_ops);
}

// ─── Внешний C++ API ─────────────────────────────────────────────────────────

void LuaCallEntityFunction(World& world, DeferredOps& deferred,
                           EntityHandle entity, const char* fnName)
{
    if (auto* host = FindHost(&world))
        host->CallEntityFunction(world, deferred, entity, fnName);
}

void LuaCallEntityFunction(World& world, DeferredOps& deferred,
                           EntityHandle entity, const char* fnName, float arg)
{
    if (auto* host = FindHost(&world))
        host->CallEntityFunction(world, deferred, entity, fnName, arg);
}

} // namespace CHEngine
