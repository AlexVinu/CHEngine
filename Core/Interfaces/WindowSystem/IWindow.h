#pragma once

#include <Core.h>
#include "EventData.h"
#include "RenderData.h"
#include <Input/KeyCodes.h>

namespace CHEngine
{
    namespace WindowCallbacks {
        using ErrorCallbackFn       = void(*)(int, const char*);
        using ResizeCallbackFn      = void(*)(void*, int, int);
        using CloseCallbackFn       = void(*)(void*);
    }

    using namespace WindowCallbacks;

    // Контекст коллбэков окна. Несёт только оконные (push) события: ресайз и
    // закрытие. Ввод клавиатуры/мыши читается через polling-методы ниже
    // (IsKeyDown / GetMousePosition / GetScrollDelta), а не через коллбэки.
    struct WindowContext
    {
        void* UserPointer = nullptr;

        ResizeCallbackFn  ResizeCallback = nullptr;
        CloseCallbackFn   CloseCallback  = nullptr;
    };

    // Платформенное окно (GLFW, Win32 и т.д.)
    // Управляет созданием окна, OpenGL-контекстом, событиями и swap buffers
    class IWindow
    {
    public:
        virtual ~IWindow() = default;

        virtual void Shutdown() = 0;

        virtual void SwapBuffers() = 0;
        virtual void PollEvents() = 0;

        virtual void SetVSync(bool enabled) = 0;

		virtual void SetMouse(bool) = 0;

        virtual void SetWindowContext(const WindowContext& context) = 0;

        virtual void* GetNativeWindow() = 0;

        virtual uint32_t GetWidth() const = 0;
        virtual uint32_t GetHeight() const = 0;

        virtual RendererInitInfo GetRenderInitInfo(ERenderAPI render_api) const = 0;
        // ── Polling input (без OS-задержки, вызывается каждый кадр) ──────────
        virtual bool  IsKeyDown(int key) const = 0;
        virtual bool  IsMouseButtonDown(int button) const = 0;
        virtual void  GetMousePosition(float& x, float& y) const = 0;
        virtual float GetScrollDelta() const  { return 0.0f; }
        virtual void  ClearScrollDelta()      {}

        // ── Window geometry (позиция и размер для сохранения/восстановления) ─
        virtual void GetWindowPos(int& x, int& y) const { x = 0; y = 0; }
        virtual void SetWindowPos(int x, int y) { (void)x; (void)y; }
        virtual void SetWindowSize(uint32_t w, uint32_t h) { (void)w; (void)h; }
    };
}
