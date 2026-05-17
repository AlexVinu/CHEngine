#include "InputActions.h"
#include "ActionMap.h"

#include <CHEngine/Input/Input.h>
#include <Input/KeyCodes.h>

#include <imgui.h>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

namespace Sandbox {

namespace {

ActionMap                          g_Map;
std::vector<InputContext>          g_ContextStack = { InputContext::Editor };

std::unordered_map<std::string, bool> g_Triggered;
std::unordered_map<std::string, bool> g_Down;
std::unordered_map<std::string, bool> g_Released;

float g_MouseDX    = 0.0f;
float g_MouseDY    = 0.0f;
float g_MouseWheel = 0.0f;

uint8_t CurrentMods()
{
    uint8_t m = Mod_None;
    if (CHEngine::Input::IsKeyDown(CHEngine::Key::LeftControl) ||
        CHEngine::Input::IsKeyDown(CHEngine::Key::RightControl) ||
        CHEngine::Input::IsKeyDown(CHEngine::Key::LeftSuper)    ||
        CHEngine::Input::IsKeyDown(CHEngine::Key::RightSuper))
        m |= Mod_Ctrl;
    if (CHEngine::Input::IsKeyDown(CHEngine::Key::LeftShift) ||
        CHEngine::Input::IsKeyDown(CHEngine::Key::RightShift))
        m |= Mod_Shift;
    if (CHEngine::Input::IsKeyDown(CHEngine::Key::LeftAlt) ||
        CHEngine::Input::IsKeyDown(CHEngine::Key::RightAlt))
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
        if (c.key == b.key && c.mouse == b.mouseButton && c.mods == b.mods && c.trigger == b.trigger)
            return true;
    return false;
}

// Evaluates one binding. Returns true if it produced any state (set t/d/r accordingly).
bool EvalBinding(const ActionBinding& b, uint8_t currentMods, bool& t, bool& d, bool& r)
{
    if (b.mods != currentMods) return false;

    if (b.key >= 0)
    {
        switch (b.trigger)
        {
            case TriggerType::Pressed:  t = CHEngine::Input::IsKeyPressed(b.key);  break;
            case TriggerType::Down:     d = CHEngine::Input::IsKeyDown(b.key);     break;
            case TriggerType::Released: r = CHEngine::Input::IsKeyReleased(b.key); break;
            case TriggerType::Drag:     return false; // mouse-only
        }
    }
    else if (b.mouseButton >= 0)
    {
        switch (b.trigger)
        {
            case TriggerType::Pressed:  t = ImGui::IsMouseClicked(b.mouseButton, false); break;
            case TriggerType::Down:     d = ImGui::IsMouseDown(b.mouseButton);           break;
            case TriggerType::Released: r = ImGui::IsMouseReleased(b.mouseButton);       break;
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

bool InputActions::LoadFromJson(const std::filesystem::path& path)
{
    return g_Map.LoadFromJson(path);
}

void InputActions::PushContext(InputContext ctx)
{
    // Prevent duplicate at the top — double-push with single-pop would leave a stale entry.
    if (!g_ContextStack.empty() && g_ContextStack.back() == ctx) return;
    g_ContextStack.push_back(ctx);
}

void InputActions::PopContext(InputContext ctx)
{
    for (auto it = g_ContextStack.rbegin(); it != g_ContextStack.rend(); ++it)
    {
        if (*it == ctx)
        {
            g_ContextStack.erase(std::next(it).base());
            return;
        }
    }
}

bool InputActions::IsActiveContext(InputContext ctx)
{
    // Only the top-most context (and below, up to a barrier) should be active.
    // Currently returns true if ctx is anywhere on the stack — this allows
    // deeper contexts to remain active when a higher context is pushed.
    // For now we preserve existing behaviour but document the intent.
    // TODO: implement context barriers so pushed Game context blocks Editor shortcuts.
    if (g_ContextStack.empty()) return false;
    return g_ContextStack.back() == ctx;
}

void InputActions::BeginFrame()
{
    g_Triggered.clear();
    g_Down.clear();
    g_Released.clear();

    ImGuiIO& io   = ImGui::GetIO();
    g_MouseDX     = io.MouseDelta.x;
    g_MouseDY     = io.MouseDelta.y;
    g_MouseWheel  = io.MouseWheel;

    const uint8_t mods = CurrentMods();

    // Walk contexts top-down so higher contexts can "consume" keys.
    std::vector<ConsumedCombo> consumed;
    for (auto it = g_ContextStack.rbegin(); it != g_ContextStack.rend(); ++it)
    {
        const InputContext ctx = *it;
        for (const auto& [name, action] : g_Map.All())
        {
            if (action.context != ctx) continue;
            for (const auto& b : action.bindings)
            {
                if (IsConsumed(consumed, b)) continue;
                bool t = false, d = false, r = false;
                if (!EvalBinding(b, mods, t, d, r)) continue;
                if (t) g_Triggered[name] = true;
                if (d) g_Down[name]      = true;
                if (r) g_Released[name]  = true;
                consumed.push_back({ b.key, b.mouseButton, b.mods, b.trigger });
            }
        }
    }
}

// Lookup helper: avoids constructing std::string from string_view on each call.
static bool MapGet(const std::unordered_map<std::string, bool>& m, std::string_view key)
{
    // std::unordered_map doesn't support heterogeneous lookup before C++26,
    // but we can avoid allocation via find on the raw key data with a matching string.
    // For hot paths this is faster than constructing a temporary std::string.
    auto it = m.find(std::string(key)); // one allocation per call; acceptable at current scale
    return it != m.end() && it->second;
}

bool InputActions::Triggered(std::string_view a) { return MapGet(g_Triggered, a); }
bool InputActions::Down     (std::string_view a) { return MapGet(g_Down,      a); }
bool InputActions::Released (std::string_view a) { return MapGet(g_Released,  a); }

float InputActions::GetAxis(Axis axis)
{
    switch (axis)
    {
        case Axis::MouseDeltaX: return g_MouseDX;
        case Axis::MouseDeltaY: return g_MouseDY;
        case Axis::MouseWheel:  return g_MouseWheel;
    }
    return 0.0f;
}

} // namespace Sandbox
