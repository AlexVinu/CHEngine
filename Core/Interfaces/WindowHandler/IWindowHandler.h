#pragma once

#include <Core.h>
#include "EventData.h"

namespace CHEngine {
	
	namespace RendererCallbacks {
		using ErrorCallbackFn = void(*)(int, const char*);
		using ResizeCallbackFn = void(*)(void*, int, int);
		using CloseCallbackFn = void(*)(void*);
		using KeyCallbackFn = void(*)(void*, int, int, int, int);
		using MouseButtonCallbackFn = void(*)(void*, int, int, int);
		using ScrollCallbackFn = void(*)(void*, float, float);
		using CursorPosCallbackFn = void(*)(void*, float, float);
	}

	using namespace RendererCallbacks;

	struct RendererWindowContext
	{
		void* UserPointer = nullptr;

		ResizeCallbackFn       ResizeCallback = nullptr;
		CloseCallbackFn        CloseCallback = nullptr;
		KeyCallbackFn          KeyCallback = nullptr;
		MouseButtonCallbackFn  MouseButtonCallback = nullptr;
		ScrollCallbackFn       ScrollCallback = nullptr;
		CursorPosCallbackFn    CursorPosCallback = nullptr;
	};

	class IWindowHandler
	{
	public:
		virtual ~IWindowHandler() = default;

		virtual void Init(const unsigned int width, const unsigned int height, const char* title, ErrorCallbackFn errorCallbackFn) = 0;
		virtual void Shutdown() = 0;

		virtual void SwapBuffers() = 0;
		virtual void PollEvents() = 0;

		virtual void SetVSync(bool enabled) = 0;

		//virtual void SetViewport(uint32_t width, uint32_t height, (void*)(uint32_t, uint32_t)) = 0;

		virtual void SetWindowContext(const RendererWindowContext& context) = 0;

		virtual void* GetNativeWindow() = 0;
	};
}