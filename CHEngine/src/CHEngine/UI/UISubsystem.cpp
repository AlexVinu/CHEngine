#include "chepch.h"
#include "UISubsystem.h"

namespace CHEngine
{
    UISubsystem::UISubsystem(IImGuiFactory* factory, IImGuiLayer* layer)
        : m_Factory(factory)
        , m_Layer(layer)
    {
        CHE_CORE_ASSERT(factory, "UISubsystem: factory must be non-null");
        CHE_CORE_ASSERT(layer,   "UISubsystem: layer must be non-null");
    }

    UISubsystem::~UISubsystem()
    {
        if (m_Layer)
        {
            m_Factory->Delete(m_Layer);
            m_Layer = nullptr;
        }
    }

    void UISubsystem::SetRenderContext(const RenderContextInfo& ctx)
    {
        m_Layer->SetRenderContext(ctx);
    }

    void UISubsystem::Begin()
    {
        m_Layer->Begin();
        static bool s_Logged = false;
        if (!s_Logged) { CHE_CORE_INFO("IMGUI: Begin Frame"); s_Logged = true; }
    }

    void UISubsystem::End()
    {
        m_Layer->End();
        static bool s_Logged = false;
        if (!s_Logged) { CHE_CORE_INFO("IMGUI: End Frame"); s_Logged = true; }
    }
}
