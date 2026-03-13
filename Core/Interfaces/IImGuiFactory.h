#pragma once

#include <Core.h>
#include "IModuleFactory.h"
#include "Render/IImGuiLayer.h"

namespace CHEngine {
    struct IImGuiFactory : IModuleFactory
    {
        virtual ~IImGuiFactory() = default;

        virtual IImGuiLayer* CreateImGuiLayer(void* nativeWindow) = 0;
        virtual void Delete(IImGuiLayer* ptr) = 0;
    };
}

DECLARE_MODULE_FACTORY()
