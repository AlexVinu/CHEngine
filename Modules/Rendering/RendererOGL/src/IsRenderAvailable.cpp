
#include "IsRenderAvailable.h"

#if defined(CHE_PLATFORM_WINDOWS)

// ========================= WINDOWS / WGL =========================
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>

namespace CHModules
{
    bool CheckIsWorking()
    {
        WNDCLASSA wc{};
        wc.style = CS_OWNDC;
        wc.lpfnWndProc = DefWindowProcA;
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.lpszClassName = "DummyOpenGLWindow";

        if (!RegisterClassA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            return false;

        HWND hwnd = CreateWindowA(
            wc.lpszClassName,
            "Dummy",
            WS_OVERLAPPEDWINDOW,
            0, 0, 1, 1,
            nullptr, nullptr,
            wc.hInstance,
            nullptr
        );

        if (!hwnd)
            return false;

        HDC dc = GetDC(hwnd);
        if (!dc)
        {
            DestroyWindow(hwnd);
            return false;
        }

        PIXELFORMATDESCRIPTOR pfd{};
        pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 24;
        pfd.cDepthBits = 24;
        pfd.cStencilBits = 8;
        pfd.iLayerType = PFD_MAIN_PLANE;

        int pf = ChoosePixelFormat(dc, &pfd);
        if (pf == 0)
        {
            ReleaseDC(hwnd, dc);
            DestroyWindow(hwnd);
            return false;
        }

        if (!SetPixelFormat(dc, pf, &pfd))
        {
            ReleaseDC(hwnd, dc);
            DestroyWindow(hwnd);
            return false;
        }

        HGLRC rc = wglCreateContext(dc);
        if (!rc)
        {
            ReleaseDC(hwnd, dc);
            DestroyWindow(hwnd);
            return false;
        }

        bool ok = wglMakeCurrent(dc, rc) == TRUE;
        if (ok)
            ok = (glGetString(GL_VERSION) != nullptr);

        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(rc);
        ReleaseDC(hwnd, dc);
        DestroyWindow(hwnd);

        return ok;
    }

    #elif defined(CHE_PLATFORM_APPLE)

    // ========================= MACOS / CGL =========================

    bool CheckIsWorking()
    {
        CGLPixelFormatAttribute attrs[] =
        {
            kCGLPFAAccelerated,
            kCGLPFAAllowOfflineRenderers,
            (CGLPixelFormatAttribute)0
        };

        CGLPixelFormatObj pixelFormat = nullptr;
        GLint numFormats = 0;

        CGLError err = CGLChoosePixelFormat(attrs, &pixelFormat, &numFormats);
        if (err != kCGLNoError || !pixelFormat || numFormats <= 0)
            return false;

        CGLContextObj context = nullptr;
        err = CGLCreateContext(pixelFormat, nullptr, &context);
        CGLReleasePixelFormat(pixelFormat);

        if (err != kCGLNoError || !context)
            return false;

        err = CGLSetCurrentContext(context);
        if (err != kCGLNoError)
        {
            CGLReleaseContext(context);
            return false;
        }

        bool ok = glGetString(GL_VERSION) != nullptr;

        CGLSetCurrentContext(nullptr);
        CGLReleaseContext(context);

        return ok;
    }

    #elif defined(CHE_PLATFORM_LINUX)

    // ========================= LINUX / X11 + GLX =========================
    #include <X11/Xlib.h>

    bool CheckIsWorking()
    {
        Display* display = XOpenDisplay(nullptr);
        if (!display)
            return false;

        int screen = DefaultScreen(display);

        static int visualAttribs[] =
        {
            GLX_RGBA,
            GLX_DEPTH_SIZE, 24,
            GLX_STENCIL_SIZE, 8,
            GLX_DOUBLEBUFFER,
            None
        };

        XVisualInfo* vi = glXChooseVisual(display, screen, visualAttribs);
        if (!vi)
        {
            XCloseDisplay(display);
            return false;
        }

        Colormap cmap = XCreateColormap(
            display,
            RootWindow(display, vi->screen),
            vi->visual,
            AllocNone
        );

        XSetWindowAttributes swa{};
        swa.colormap = cmap;
        swa.event_mask = StructureNotifyMask;

        Window win = XCreateWindow(
            display,
            RootWindow(display, vi->screen),
            0, 0, 1, 1,
            0,
            vi->depth,
            InputOutput,
            vi->visual,
            CWColormap | CWEventMask,
            &swa
        );

        if (!win)
        {
            XFree(vi);
            XCloseDisplay(display);
            return false;
        }

        GLXContext ctx = glXCreateContext(display, vi, nullptr, True);
        if (!ctx)
        {
            XDestroyWindow(display, win);
            XFreeColormap(display, cmap);
            XFree(vi);
            XCloseDisplay(display);
            return false;
        }

        bool ok = glXMakeCurrent(display, win, ctx) == True;
        if (ok)
            ok = (glGetString(GL_VERSION) != nullptr);

        glXMakeCurrent(display, None, nullptr);
        glXDestroyContext(display, ctx);
        XDestroyWindow(display, win);
        XFreeColormap(display, cmap);
        XFree(vi);
        XCloseDisplay(display);

        return ok;
    }

    #else

    bool CheckIsWorking()
    {
        return false;
    }

    #endif
}
