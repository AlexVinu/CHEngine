#pragma once

#include "UISystem/IImGuiFactory.h"
#include "ImGuiLayerGLFW_OGL.h"

namespace CHModules
{
    struct ImGuiFactoryGLFW_OGL : public CHEngine::IImGuiFactory
    {
        virtual ~ImGuiFactoryGLFW_OGL() = default;

        virtual CHEngine::IImGuiLayer* Create(void* nativeWindow) override;

        virtual void Delete(CHEngine::IImGuiLayer* layer) override;

        virtual CHEngine::ModuleType GetType() const override;
    };
}

