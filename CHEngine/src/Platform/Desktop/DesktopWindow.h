//#pragma once
//
//#include "CHEngine/Window.h"
//
//namespace CHEngine {
//
//	class DesktopWindow : public Window
//	{
//	public:
//		DesktopWindow(const WindowProps& props, IRenderFactory* render_factory);
//		virtual ~DesktopWindow();
//
//		void OnUpdate() override;
//
//		inline unsigned int GetWidth() const override { return m_Data.Width; }
//		inline unsigned int GetHeight() const override { return m_Data.Height; }
//
//		// Window attributes
//		inline void SetEventCallback(const EventCallbackFn& callback) override { m_Data.EventCallback = callback; }
//		void SetVSync(bool enabled) override;
//		bool IsVSync() const override;
//		void* GetNativeWindow() const override;
//
//	private:
//		virtual void Init(const WindowProps& props, IRenderFactory* render_factory);
//		virtual void Shutdown();
//
//		struct WindowData
//		{
//			std::string Title;
//			unsigned int Width, Height;
//			bool VSync;
//
//			EventCallbackFn EventCallback;
//		};
//
//		WindowData m_Data;
//		IRenderer* m_Renderer;
//	};
//}
