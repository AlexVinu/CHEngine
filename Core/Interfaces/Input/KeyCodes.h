#pragma once

// Key codes — значения совпадают с GLFW для нулевого оверхеда конвертации.
// Используй CHEngine::Key::W, Key::LeftShift и т.д.

namespace CHEngine {

    namespace Key {
        enum Code : int
        {
            // Printable keys
            Space        = 32,
            Apostrophe   = 39,
            Comma        = 44,
            Minus        = 45,
            Period       = 46,
            Slash        = 47,

            D0 = 48, D1, D2, D3, D4, D5, D6, D7, D8, D9,

            Semicolon    = 59,
            Equal        = 61,

            A = 65, B, C, D, E, F, G, H, I, J, K, L, M,
            N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

            LeftBracket  = 91,
            Backslash    = 92,
            RightBracket = 93,
            GraveAccent  = 96,

            // Function keys
            Escape       = 256,
            Enter        = 257,
            Tab          = 258,
            Backspace    = 259,
            Insert       = 260,
            Delete       = 261,
            Right        = 262,
            Left         = 263,
            Down         = 264,
            Up           = 265,
            PageUp       = 266,
            PageDown     = 267,
            Home         = 268,
            End          = 269,
            CapsLock     = 280,
            ScrollLock   = 281,
            NumLock      = 282,
            PrintScreen  = 283,
            Pause        = 284,

            F1  = 290, F2,  F3,  F4,  F5,  F6,
            F7  = 296, F8,  F9,  F10, F11, F12,

            // Numpad
            KP0 = 320, KP1, KP2, KP3, KP4,
            KP5 = 325, KP6, KP7, KP8, KP9,
            KPDecimal  = 330,
            KPDivide   = 331,
            KPMultiply = 332,
            KPSubtract = 333,
            KPAdd      = 334,
            KPEnter    = 335,
            KPEqual    = 336,

            // Modifiers
            LeftShift    = 340,
            LeftControl  = 341,
            LeftAlt      = 342,
            LeftSuper    = 343,  // Cmd на macOS, Win на Windows
            RightShift   = 344,
            RightControl = 345,
            RightAlt     = 346,
            RightSuper   = 347,
            Menu         = 348,

            Last         = Menu
        };
    }

    namespace MouseButton {
        enum Code : int
        {
            Button0 = 0,
            Button1 = 1,
            Button2 = 2,
            Button3 = 3,
            Button4 = 4,
            Button5 = 5,
            Button6 = 6,
            Button7 = 7,

            Left   = Button0,
            Right  = Button1,
            Middle = Button2,

            Last   = Button7
        };
    }

} // namespace CHEngine
