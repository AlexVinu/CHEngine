#include "chepch.h"
#include "Input.h"

#include <Render/IWindow.h>

namespace CHEngine {

    // ── Статические данные ────────────────────────────────────────────────────
    bool  Input::s_CurKeys[KEY_COUNT]   = {};
    bool  Input::s_PrevKeys[KEY_COUNT]  = {};
    bool  Input::s_CurMouse[MOUSE_COUNT]  = {};
    bool  Input::s_PrevMouse[MOUSE_COUNT] = {};
    float Input::s_MouseX     = 0.0f;
    float Input::s_MouseY     = 0.0f;
    float Input::s_PrevMouseX = 0.0f;
    float Input::s_PrevMouseY = 0.0f;

    // ─────────────────────────────────────────────────────────────────────────
    void Input::BeginFrame(IWindow* window)
    {
        if (!window) return;

        // Сохраняем прошлый кадр
        std::memcpy(s_PrevKeys,  s_CurKeys,  sizeof(s_CurKeys));
        std::memcpy(s_PrevMouse, s_CurMouse, sizeof(s_CurMouse));
        s_PrevMouseX = s_MouseX;
        s_PrevMouseY = s_MouseY;

        // Опрашиваем текущее состояние через IWindow (реализация в WindowGLFW)
        for (int k = 0; k < KEY_COUNT; ++k)
            s_CurKeys[k] = window->IsKeyDown(k);

        for (int b = 0; b < MOUSE_COUNT; ++b)
            s_CurMouse[b] = window->IsMouseButtonDown(b);

        window->GetMousePosition(s_MouseX, s_MouseY);
    }

    // ── Клавиатура ────────────────────────────────────────────────────────────
    bool Input::IsKeyDown(int key)
    {
        if (key < 0 || key >= KEY_COUNT) return false;
        return s_CurKeys[key];
    }

    bool Input::IsKeyPressed(int key)
    {
        if (key < 0 || key >= KEY_COUNT) return false;
        return s_CurKeys[key] && !s_PrevKeys[key];
    }

    bool Input::IsKeyReleased(int key)
    {
        if (key < 0 || key >= KEY_COUNT) return false;
        return !s_CurKeys[key] && s_PrevKeys[key];
    }

    // ── Мышь ──────────────────────────────────────────────────────────────────
    bool Input::IsMouseButtonDown(int button)
    {
        if (button < 0 || button >= MOUSE_COUNT) return false;
        return s_CurMouse[button];
    }

    bool Input::IsMouseButtonPressed(int button)
    {
        if (button < 0 || button >= MOUSE_COUNT) return false;
        return s_CurMouse[button] && !s_PrevMouse[button];
    }

    bool Input::IsMouseButtonReleased(int button)
    {
        if (button < 0 || button >= MOUSE_COUNT) return false;
        return !s_CurMouse[button] && s_PrevMouse[button];
    }

    float Input::GetMouseX()      { return s_MouseX; }
    float Input::GetMouseY()      { return s_MouseY; }
    float Input::GetMouseDeltaX() { return s_MouseX - s_PrevMouseX; }
    float Input::GetMouseDeltaY() { return s_MouseY - s_PrevMouseY; }

} // namespace CHEngine
