#pragma once

#include "UISystem/IImGuiLayer.h"

namespace CHModules {

    class ImGuiLayerGLFW_OGL : public CHEngine::IImGuiLayer
    {
    public:
        ImGuiLayerGLFW_OGL(void* nativeWindow);
        ~ImGuiLayerGLFW_OGL() override;

        void Begin() override;
        void End() override;
    };

}

