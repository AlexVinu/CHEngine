#pragma once

#include "Core.h"
#include "UISystem/IImGuiFactory.h"

namespace CHEngine
{
    // Owns the ImGui layer.  Constructed only when ImGui is available;
    // Application holds it as Scope<UISubsystem> (nullptr = ImGui disabled).
    class CHENGINE_API UISubsystem
    {
    public:
        // factory must outlive this object (lifetime = module).
        UISubsystem(IImGuiFactory* factory, IImGuiLayer* layer);
        ~UISubsystem();  // calls factory->Delete(layer)

        UISubsystem(const UISubsystem&) = delete;
        UISubsystem& operator=(const UISubsystem&) = delete;

        IImGuiLayer* GetLayer() const { return m_Layer; }

        void SetRenderContext(const RenderContextInfo& ctx);
        void Begin();
        void End();

    private:
        IImGuiFactory* m_Factory = nullptr;
        IImGuiLayer*   m_Layer   = nullptr;
    };
}
