#pragma once

#include "UISystem/IImGuiLayer.h"

namespace CHModules {

    class ImGuiLayerOGL : public CHEngine::IImGuiLayer
    {
    public:
        ImGuiLayerOGL(void* nativeWindow);
        ~ImGuiLayerOGL() override;

        void Begin() override;
        void End() override;
    };

}
