#pragma once

#include <Core.h>

#include "Render/IFrameGraphBackend.h"

#include <string>
#include <vector>

namespace CHEngine {

    class CHENGINE_API IFrameGraphFrontend
    {
    public:
        virtual ~IFrameGraphFrontend() = default;

        virtual void Reset() = 0;
        virtual RGTextureHandle ImportTexture(TextureHandle texture, const RGTextureDesc& desc) = 0;
        virtual RGTextureHandle CreateTransient(const RGTextureDesc& desc) = 0;
        virtual PassRef AddPass(std::string name, BuildFn build, ExecuteFn execute) = 0;
        virtual void Compile() = 0;
        virtual void Execute(IFrameGraphBackend& backend) = 0;

        virtual const std::vector<CompiledPass>& GetCompiledPasses() const = 0;
    };

} // namespace CHEngine
