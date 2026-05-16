#include "chepch.h"
#include "LuaScriptSystem.h"

// sol2 — подключаем только здесь (PIMPL)
// SOL_ALL_SAFETIES_ON включает SOL_SAFE_REFERENCES, что добавляет type-check
// в конструкторы sol::table/sol::environment и вызывает lua_error() без pcall-защиты
// (индекс LUA_REGISTRYINDEX = -1001000 → PANIC). Используем точечные флаги вместо всех сразу.
#define SOL_SAFE_USERTYPE   1   // проверяем типы usertype при вызове методов
#define SOL_SAFE_FUNCTION   1   // ошибки функций через safe_function
#define SOL_SAFE_GETTER     1   // геттеры проверяют типы
#define SOL_SAFE_NUMERICS   0   // числовые преобразования — не нужна проверка
#define SOL_SAFE_REFERENCES 0   // ВЫКЛЮЧАЕМ: конструкторы reference вызывают
                                // lua_error на LUA_REGISTRYINDEX → unprotected PANIC
#include <sol/sol.hpp>

#include "CHEngine/World/World.h"
#include "CHEngine/Scene/Scene.h"
#include "CHEngine/Scene/Entity.h"
#include "CHEngine/Scene/Components.h"
#include "CHEngine/Input/Input.h"
#include <Input/KeyCodes.h>
#include "Log/Log.h"

#include <filesystem>
#include <unordered_map>
#include <string>

namespace CHEngine {

// ─────────────────────────────────────────────────────────────────────────────
// ScriptEntity — враппер вокруг EntityHandle + World* для Lua API
// ─────────────────────────────────────────────────────────────────────────────
struct ScriptEntity
{
    EntityHandle handle;
    World*       world = nullptr;

    // Вспомогательный метод — получить Entity* из сцены
    Entity* GetEntity() const
    {
        auto scene = world->GetSceneRef();
        return scene ? scene->TryGetEntity(handle) : nullptr;
    }

    // ── Имя ───────────────────────────────────────────────────────────────────
    std::string GetName() const
    {
        if (auto* e = GetEntity(); e && e->HasComponent<TagComponent>())
            return e->GetComponent<TagComponent>().Name;
        return "Unknown";
    }

    // ── Позиция ───────────────────────────────────────────────────────────────
    sol::table GetPosition(sol::this_state s) const
    {
        sol::state_view lua(s);
        auto tbl = lua.create_table();
        if (auto* e = GetEntity(); e && e->HasComponent<TransformComponent>())
        {
            auto& p = e->GetComponent<TransformComponent>().ObjectTransform.Position;
            tbl["x"] = p.x; tbl["y"] = p.y; tbl["z"] = p.z;
        }
        return tbl;
    }

    void SetPosition(float x, float y, float z) const
    {
        if (auto* e = GetEntity(); e && e->HasComponent<TransformComponent>())
        {
            auto& p = e->GetComponent<TransformComponent>().ObjectTransform.Position;
            p.x = x; p.y = y; p.z = z;
        }
    }

    // ── Поворот (Euler degrees) ───────────────────────────────────────────────
    sol::table GetRotation(sol::this_state s) const
    {
        sol::state_view lua(s);
        auto tbl = lua.create_table();
        if (auto* e = GetEntity(); e && e->HasComponent<TransformComponent>())
        {
            auto& r = e->GetComponent<TransformComponent>().ObjectTransform.Rotation;
            tbl["x"] = r.x; tbl["y"] = r.y; tbl["z"] = r.z;
        }
        return tbl;
    }

    void SetRotation(float x, float y, float z) const
    {
        if (auto* e = GetEntity(); e && e->HasComponent<TransformComponent>())
        {
            auto& r = e->GetComponent<TransformComponent>().ObjectTransform.Rotation;
            r.x = x; r.y = y; r.z = z;
        }
    }

    // ── Масштаб ───────────────────────────────────────────────────────────────
    sol::table GetScale(sol::this_state s) const
    {
        sol::state_view lua(s);
        auto tbl = lua.create_table();
        if (auto* e = GetEntity(); e && e->HasComponent<TransformComponent>())
        {
            auto& sc = e->GetComponent<TransformComponent>().ObjectTransform.Scale;
            tbl["x"] = sc.x; tbl["y"] = sc.y; tbl["z"] = sc.z;
        }
        return tbl;
    }

    void SetScale(float x, float y, float z) const
    {
        if (auto* e = GetEntity(); e && e->HasComponent<TransformComponent>())
        {
            auto& sc = e->GetComponent<TransformComponent>().ObjectTransform.Scale;
            sc.x = x; sc.y = y; sc.z = z;
        }
    }

    // ── Цвет ─────────────────────────────────────────────────────────────────
    sol::table GetColor(sol::this_state s) const
    {
        sol::state_view lua(s);
        auto tbl = lua.create_table();
        if (auto* e = GetEntity(); e && e->HasComponent<ColorComponent>())
        {
            auto& c = e->GetComponent<ColorComponent>().Color;
            tbl["r"] = c.r; tbl["g"] = c.g; tbl["b"] = c.b; tbl["a"] = c.a;
        }
        return tbl;
    }

    void SetColor(float r, float g, float b, float a) const
    {
        auto* e = GetEntity();
        if (!e) return;
        // Add ColorComponent if missing (mirrors editor behaviour)
        if (!e->HasComponent<ColorComponent>())
            e->AddComponent<ColorComponent>();
        e->GetComponent<ColorComponent>().Color = { r, g, b, a };
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Impl — хранит sol::state и инстансы скриптов
// ─────────────────────────────────────────────────────────────────────────────
// Кастомный Lua panic handler: вместо abort() — логируем и бросаем исключение.
// Перехватывается снаружи через try/catch чтобы движок не падал.
static int LuaPanicHandler(lua_State* L)
{
    const char* msg = lua_tostring(L, -1);
    CHE_CORE_CRITICAL("[LuaScriptSystem] Lua PANIC: {}", msg ? msg : "(unknown error)");
    throw std::runtime_error(msg ? msg : "Lua panic");
}

struct LuaScriptSystem::Impl
{
    sol::state lua;

    Impl()
    {
        // Переопределяем panic handler: без этого lua_error() вне pcall → abort().
        lua_atpanic(lua.lua_state(), &LuaPanicHandler);
    }

    struct ScriptInstance
    {
        sol::environment   env;
        sol::safe_function onStart;
        sol::safe_function onUpdate;
        sol::safe_function onStop;
        bool started = false;
    };

    std::unordered_map<uint64_t, ScriptInstance> instances;

    // ── Регистрация всего API движка ─────────────────────────────────────────
    void SetupAPI()
    {
        lua.open_libraries(
            sol::lib::base,
            sol::lib::math,
            sol::lib::string,
            sol::lib::table,
            sol::lib::io,
            sol::lib::os
        );

        // ── Entity usertype ───────────────────────────────────────────────────
        lua.new_usertype<ScriptEntity>("Entity",
            "GetName",     &ScriptEntity::GetName,
            "GetPosition", &ScriptEntity::GetPosition,
            "SetPosition", &ScriptEntity::SetPosition,
            "GetRotation", &ScriptEntity::GetRotation,
            "SetRotation", &ScriptEntity::SetRotation,
            "GetScale",    &ScriptEntity::GetScale,
            "SetScale",    &ScriptEntity::SetScale,
            "GetColor",    &ScriptEntity::GetColor,
            "SetColor",    &ScriptEntity::SetColor
        );

        // ── Input ─────────────────────────────────────────────────────────────
        auto inputTable = lua.create_named_table("Input");
        inputTable["IsKeyDown"]            = [](int key) { return Input::IsKeyDown(key); };
        inputTable["IsKeyPressed"]         = [](int key) { return Input::IsKeyPressed(key); };
        inputTable["IsKeyReleased"]        = [](int key) { return Input::IsKeyReleased(key); };
        inputTable["IsMouseButtonDown"]    = [](int btn) { return Input::IsMouseButtonDown(btn); };
        inputTable["IsMouseButtonPressed"] = [](int btn) { return Input::IsMouseButtonPressed(btn); };
        inputTable["GetMouseX"]            = [] { return Input::GetMouseX(); };
        inputTable["GetMouseY"]            = [] { return Input::GetMouseY(); };
        inputTable["GetMouseDeltaX"]       = [] { return Input::GetMouseDeltaX(); };
        inputTable["GetMouseDeltaY"]       = [] { return Input::GetMouseDeltaY(); };

        // ── Key constants ─────────────────────────────────────────────────────
        auto K = lua.create_named_table("Key");
        K["Space"]     = (int)Key::Space;
        K["A"]=(int)Key::A; K["B"]=(int)Key::B; K["C"]=(int)Key::C; K["D"]=(int)Key::D;
        K["E"]=(int)Key::E; K["F"]=(int)Key::F; K["G"]=(int)Key::G; K["H"]=(int)Key::H;
        K["I"]=(int)Key::I; K["J"]=(int)Key::J; K["K"]=(int)Key::K; K["L"]=(int)Key::L;
        K["M"]=(int)Key::M; K["N"]=(int)Key::N; K["O"]=(int)Key::O; K["P"]=(int)Key::P;
        K["Q"]=(int)Key::Q; K["R"]=(int)Key::R; K["S"]=(int)Key::S; K["T"]=(int)Key::T;
        K["U"]=(int)Key::U; K["V"]=(int)Key::V; K["W"]=(int)Key::W; K["X"]=(int)Key::X;
        K["Y"]=(int)Key::Y; K["Z"]=(int)Key::Z;
        K["Escape"]     = (int)Key::Escape;
        K["Enter"]      = (int)Key::Enter;
        K["Tab"]        = (int)Key::Tab;
        K["Backspace"]  = (int)Key::Backspace;
        K["Left"]       = (int)Key::Left;
        K["Right"]      = (int)Key::Right;
        K["Up"]         = (int)Key::Up;
        K["Down"]       = (int)Key::Down;
        K["LeftShift"]  = (int)Key::LeftShift;
        K["LeftCtrl"]   = (int)Key::LeftControl;
        K["F1"]=(int)Key::F1;  K["F2"]=(int)Key::F2;  K["F3"]=(int)Key::F3;
        K["F4"]=(int)Key::F4;  K["F5"]=(int)Key::F5;  K["F12"]=(int)Key::F12;

        // ── Log ───────────────────────────────────────────────────────────────
        auto logTable = lua.create_named_table("Log");
        logTable["Info"]  = [](const std::string& msg) { CHE_CORE_INFO("[Lua] {}", msg); };
        logTable["Warn"]  = [](const std::string& msg) { CHE_CORE_WARN("[Lua] {}", msg); };
        logTable["Error"] = [](const std::string& msg) { CHE_CORE_ERROR("[Lua] {}", msg); };
    }

    // ── Загрузка скрипта в изолированное окружение ───────────────────────────
    bool LoadScript(uint64_t key, const std::string& path)
    {
        if (!std::filesystem::exists(path))
        {
            CHE_CORE_ERROR("[LuaScriptSystem] Script not found: {}", path);
            return false;
        }

        sol::environment env(lua, sol::create, lua.globals());

        auto result = lua.safe_script_file(path, env, sol::script_pass_on_error);
        if (!result.valid())
        {
            sol::error err = result;
            CHE_CORE_ERROR("[LuaScriptSystem] Error loading '{}': {}", path, err.what());
            return false;
        }

        ScriptInstance inst;
        inst.env = std::move(env);

        auto fnStart  = inst.env.get<sol::object>("OnStart");
        auto fnUpdate = inst.env.get<sol::object>("OnUpdate");
        auto fnStop   = inst.env.get<sol::object>("OnStop");

        if (fnStart.is<sol::safe_function>())  inst.onStart  = fnStart.as<sol::safe_function>();
        if (fnUpdate.is<sol::safe_function>()) inst.onUpdate = fnUpdate.as<sol::safe_function>();
        if (fnStop.is<sol::safe_function>())   inst.onStop   = fnStop.as<sol::safe_function>();

        instances[key] = std::move(inst);
        return true;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// LuaScriptSystem
// ─────────────────────────────────────────────────────────────────────────────
LuaScriptSystem::LuaScriptSystem(uint8_t priority)
    : ISystem(SystemPhase::Simulation, priority)
    , m_Impl(std::make_unique<Impl>())
{
    m_Impl->SetupAPI();
}

LuaScriptSystem::~LuaScriptSystem() = default;

// ── OnBegin: загрузка скриптов + OnStart ─────────────────────────────────────
void LuaScriptSystem::OnBegin(World& world, DeferredOps& /*deferred_ops*/)
{
    m_Impl->instances.clear();

    auto scene = world.GetSceneRef();
    if (!scene) return;

    scene->ForEach<ScriptComponent>(
        [&](EntityHandle handle, const UUID&, ScriptComponent& sc)
        {
            if (!sc.Enabled || sc.ScriptPath.empty()) return;

            uint64_t key = MakeKey(handle);

            // Оборачиваем в try/catch — LuaPanicHandler бросает std::runtime_error
            // при unprotected lua_error (например, sol2 safety checks).
            try
            {
                if (!m_Impl->LoadScript(key, sc.ScriptPath)) return;

                auto& inst = m_Impl->instances[key];
                if (inst.onStart.valid())
                {
                    ScriptEntity se{ handle, &world };
                    auto res = inst.onStart(se);
                    if (!res.valid())
                    {
                        sol::error err = res;
                        CHE_CORE_ERROR("[LuaScriptSystem] OnStart error in '{}': {}", sc.ScriptPath, err.what());
                    }
                }
                inst.started = true;
            }
            catch (const std::exception& ex)
            {
                CHE_CORE_ERROR("[LuaScriptSystem] Exception in OnBegin for '{}': {}", sc.ScriptPath, ex.what());
                sc.Enabled = false;
            }
        });

    CHE_CORE_INFO("[LuaScriptSystem] Loaded {} script(s)", m_Impl->instances.size());
}

// ── Run: OnUpdate каждый кадр ────────────────────────────────────────────────
void LuaScriptSystem::Run(World& world, DeferredOps& /*deferred_ops*/, Timestep dt)
{
    auto scene = world.GetSceneRef();
    if (!scene) return;

    scene->ForEach<ScriptComponent>(
        [&](EntityHandle handle, const UUID&, ScriptComponent& sc)
        {
            if (!sc.Enabled) return;

            auto it = m_Impl->instances.find(MakeKey(handle));
            if (it == m_Impl->instances.end()) return;

            auto& inst = it->second;
            if (!inst.started || !inst.onUpdate.valid()) return;

            try
            {
                ScriptEntity se{ handle, &world };
                auto res = inst.onUpdate(se, (float)dt);
                if (!res.valid())
                {
                    sol::error err = res;
                    CHE_CORE_ERROR("[LuaScriptSystem] OnUpdate error in '{}': {}", sc.ScriptPath, err.what());
                    sc.Enabled = false;
                }
            }
            catch (const std::exception& ex)
            {
                CHE_CORE_ERROR("[LuaScriptSystem] Exception in OnUpdate for '{}': {}", sc.ScriptPath, ex.what());
                sc.Enabled = false;
            }
        });
}

// ── OnEnd: OnStop + очистка ───────────────────────────────────────────────────
void LuaScriptSystem::OnEnd(World& world, DeferredOps& /*deferred_ops*/)
{
    auto scene = world.GetSceneRef();
    if (scene)
    {
        scene->ForEach<ScriptComponent>(
            [&](EntityHandle handle, const UUID&, ScriptComponent& sc)
            {
                auto it = m_Impl->instances.find(MakeKey(handle));
                if (it == m_Impl->instances.end()) return;

                auto& inst = it->second;
                if (inst.started && inst.onStop.valid())
                {
                    ScriptEntity se{ handle, &world };
                    auto res = inst.onStop(se);
                    if (!res.valid())
                    {
                        sol::error err = res;
                        CHE_CORE_ERROR("[LuaScriptSystem] OnStop error in '{}': {}", sc.ScriptPath, err.what());
                    }
                }
                (void)sc;
            });
    }

    m_Impl->instances.clear();
    CHE_CORE_INFO("[LuaScriptSystem] Scripts stopped and cleared");
}

} // namespace CHEngine
