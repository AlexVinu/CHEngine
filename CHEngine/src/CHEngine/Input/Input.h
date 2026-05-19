#pragma once

#include "Core.h"
#include <Input/KeyCodes.h>

namespace CHEngine {

    class IWindow;

    // ─────────────────────────────────────────────────────────────────────────
    // Input — статический фасад для опроса состояния клавиатуры и мыши.
    //
    // Вместо GLFW-событий (которые идут через OS с задержкой ~0.5 с) мы
    // вызываем glfwGetKey/glfwGetMouseButton через IWindow каждый кадр.
    // Это даёт мгновенный отклик без задержек.
    //
    // Использование:
    //   if (Input::IsKeyDown(Key::W))    { /* W зажата */ }
    //   if (Input::IsKeyPressed(Key::F)) { /* F нажата ИМЕННО в этом кадре */ }
    //   if (Input::IsKeyReleased(Key::Q)){ /* Q отпущена в этом кадре */ }
    // ─────────────────────────────────────────────────────────────────────────
    class Input
    {
    public:
        // Вызывается Application::Run() в начале каждого кадра.
        // window — текущее окно движка (IWindow*).
        static void BeginFrame(IWindow* window);

        // ── Клавиатура ────────────────────────────────────────────────────────
        // Зажата ли клавиша прямо сейчас
        static bool IsKeyDown(int key);
        // Нажата ли клавиша именно в этом кадре (не было в прошлом)
        static bool IsKeyPressed(int key);
        // Отпущена ли клавиша именно в этом кадре
        static bool IsKeyReleased(int key);

        // ── Мышь ──────────────────────────────────────────────────────────────
        static bool IsMouseButtonDown(int button);
        static bool IsMouseButtonPressed(int button);
        static bool IsMouseButtonReleased(int button);

        static float GetMouseX();
        static float GetMouseY();
        static float GetMouseDeltaX();
        static float GetMouseDeltaY();
        static float GetMouseWheel();

    private:
        static constexpr int KEY_COUNT = Key::Last + 1;
        static constexpr int MOUSE_COUNT = MouseButton::Last + 1;

        static bool s_CurKeys[KEY_COUNT];
        static bool s_PrevKeys[KEY_COUNT];

        static bool s_CurMouse[MOUSE_COUNT];
        static bool s_PrevMouse[MOUSE_COUNT];

        static float s_MouseX;
        static float s_MouseY;
        static float s_PrevMouseX;
        static float s_PrevMouseY;
        static float s_MouseWheel;
    };

} // namespace CHEngine
