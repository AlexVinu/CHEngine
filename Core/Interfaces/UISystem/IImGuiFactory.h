#pragma once

#include <Core.h>

#include "IImGuiLayer.h"
#include "IModuleFactory.h"

namespace CHEngine {

    struct IImGuiFactory : IModuleFactory
    {
        virtual ~IImGuiFactory() = default;

        virtual IImGuiLayer* Create(void* nativeWindow) = 0;
        virtual void Delete(IImGuiLayer* layer) = 0;

        ModuleType GetType() const override { return ModuleType::ImGui; }
    };
}

