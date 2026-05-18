#include "InputSystem.h"

#include <CHEngine/Input/Input.h>
#include <Input/KeyCodes.h>

#include <imgui.h>

#include <algorithm>
#include <cassert>
#include <string>

namespace CHEngine {

// ── Module-level registration ─────────────────────────────────────────────────

namespace {
    InputSystem* s_Instance = nullptr;
}

void RegisterInputSystem(InputSystem* sys)
{
    s_Instance = sys;
}

InputSystem& GetInputSystem()
{
    assert(s_Instance && "InputSystem not registered — call RegisterInputSystem() first");
    return *s_Instance;
}

// ── InputSystem implementation ────────────────────────────────────────────────

InputSystem::InputSystem()
    : m_ContextStack{ std::string(InputContexts::Editor) }
{
}

bool InputSystem::LoadFromJson(const std::filesystem::path& path)
{
    return m_Map.LoadFromJson(path);
}

// ── Context stack ─────────────────────────────────────────────────────────────

void InputSystem::PushContext(std::string_view ctx)
{
    m_ContextStack.emplace_back(ctx);
}

void InputSystem::PopContext(std::string_view ctx)
{
    for (auto it = m_ContextStack.rbegin(); it != m_ContextStack.rend(); ++it)
    {
        if (*it == ctx)
        {
            m_ContextStack.erase(std::next(it).base());
            return;
        }
    }
}

bool InputSystem::IsActiveContext(std::string_view ctx) const
{
    return std::find(m_ContextStack.begin(), m_ContextStack.end(), ctx)
           != m_ContextStack.end();
}

// ── BeginFrame ────────────────────────────────────────────────────────────────

namespace {

uint8_t CurrentMods()
{
    uint8_t m = Mod_None;
    if (Input::IsKeyDown(Key::LeftControl)  ||
        Input::IsKeyDown(Key::RightControl) ||
        Input::IsKeyDown(Key::LeftSuper)    ||
        Input::IsKeyDown(Key::RightSuper))
        m |= Mod_Ctrl;
    if (Input::IsKeyDown(Key::LeftShift) ||
        Input::IsKeyDown(Key::RightShift))
        m |= Mod_Shift;
    if (Input::IsKeyDown(Key::LeftAlt) ||
        Input::IsKeyDown(Key::RightAlt))
        m |= Mod_Alt;
    return m;
}

struct ConsumedCombo {
    int         key;
    int         mouse;
    uint8_t     mods;
    TriggerType trigger;
};

bool IsConsumed(const std::vector<ConsumedCombo>& v, const ActionBinding& b)
{
    for (const auto& c : v)
        if (c.key == b.key && c.mouse == b.mouseButton
            && c.mods == b.mods && c.trigger == b.trigger)
            return true;
    return false;
}

bool EvalBinding(const ActionBinding& b, uint8_t currentMods, bool& t, bool& d, bool& r)
{
    if (b.mods != currentMods) return false;

    if (b.key >= 0)
    {
        switch (b.trigger)
        {
            case TriggerType::Pressed:  t = Input::IsKeyPressed(b.key);  break;
            case TriggerType::Down:     d = Input::IsKeyDown(b.key);     break;
            case TriggerType::Released: r = Input::IsKeyReleased(b.key); break;
            case TriggerType::Drag:     return false;
        }
    }
    else if (b.mouseButton >= 0)
    {
        switch (b.trigger)
        {
            case TriggerType::Pressed:  t = ImGui::IsMouseClicked(b.mouseButton, false);            break;
            case TriggerType::Down:     d = ImGui::IsMouseDown(b.mouseButton);                      break;
            case TriggerType::Released: r = ImGui::IsMouseReleased(b.mouseButton);                  break;
            case TriggerType::Drag:     d = ImGui::IsMouseDragging(b.mouseButton, b.dragThreshold); break;
        }
    }
    else
    {
        return false;
    }
    return t || d || r;
}

} // namespace

void InputSystem::BeginFrame()
{
    m_Triggered.clear();
    m_Down.clear();
    m_Released.clear();

    ImGuiIO& io   = ImGui::GetIO();
    m_MouseDX     = io.MouseDelta.x;
    m_MouseDY     = io.MouseDelta.y;
    m_MouseWheel  = io.MouseWheel;

    const uint8_t mods = CurrentMods();
    m_CurrentMods = mods;

    std::vector<ConsumedCombo> consumed;
    for (auto it = m_ContextStack.rbegin(); it != m_ContextStack.rend(); ++it)
    {
        const std::string& ctx = *it;
        for (const auto& [name, action] : m_Map.All())
        {
            if (action.context != ctx) continue;
            for (const auto& b : action.bindings)
            {
                if (IsConsumed(consumed, b)) continue;
                bool t = false, d = false, r = false;
                if (!EvalBinding(b, mods, t, d, r)) continue;
                if (t) m_Triggered[name] = true;
                if (d) m_Down[name]      = true;
                if (r) m_Released[name]  = true;
                consumed.push_back({ b.key, b.mouseButton, b.mods, b.trigger });
            }
        }
    }
}

// ── Queries ───────────────────────────────────────────────────────────────────

namespace {
bool MapGet(const std::unordered_map<std::string, bool>& m, std::string_view key)
{
    auto it = m.find(std::string(key));
    return it != m.end() && it->second;
}
} // namespace

bool  InputSystem::Triggered(std::string_view a) const { return MapGet(m_Triggered, a); }
bool  InputSystem::Down     (std::string_view a) const { return MapGet(m_Down,      a); }
bool  InputSystem::Released (std::string_view a) const { return MapGet(m_Released,  a); }

bool  InputSystem::IsModifierDown(uint8_t mods) const { return (m_CurrentMods & mods) == mods; }

float InputSystem::GetAxis(Axis axis) const
{
    switch (axis)
    {
        case Axis::MouseDeltaX: return m_MouseDX;
        case Axis::MouseDeltaY: return m_MouseDY;
        case Axis::MouseWheel:  return m_MouseWheel;
    }
    return 0.0f;
}

} // namespace CHEngine
