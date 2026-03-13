#pragma once

#include "IImGuiFactory.h"
#include "ImGuiLayerOGL.h"

namespace CHModules {

    struct ImGuiFactoryOGL : CHEngine::IImGuiFactory
    {
        CHEngine::IImGuiLayer* CreateImGuiLayer(void* nativeWindow) override
        {
            return CreateImpl<ImGuiLayerOGL>(nativeWindow);
        }

        void Delete(CHEngine::IImGuiLayer* ptr) override
        {
            DestroyImpl(static_cast<ImGuiLayerOGL*>(ptr));
        }

        CHEngine::ModuleType GetType() const override
        {
            return CHEngine::ModuleType::ImGui;
        }
    };

}
