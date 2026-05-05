#pragma once

#include "Descriptors.h"
#include "Handles.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace CHEngine {

    class ICommandList;

    using RGTextureHandle = uint32_t;
    using PassRef = uint32_t;

    inline constexpr RGTextureHandle RG_INVALID_TEXTURE = 0xFFFFFFFFu;
    inline constexpr PassRef RG_INVALID_PASS = 0xFFFFFFFFu;

    class IRGContext
    {
    public:
        virtual ~IRGContext() = default;

        virtual TextureHandle GetTexture(RGTextureHandle handle) const = 0;
        virtual bool IsImported(RGTextureHandle handle) const = 0;
    };

    class IRGPassBuilder
    {
    public:
        virtual ~IRGPassBuilder() = default;

        virtual void Read(RGTextureHandle handle) = 0;
        virtual void Write(RGTextureHandle handle) = 0;
    };

    using BuildFn = std::function<void(IRGPassBuilder&)>;
    using ExecuteFn = std::function<void(IRGContext&, ICommandList&)>;

    struct RGCompiledResource
    {
        RGTextureHandle Handle = RG_INVALID_TEXTURE;
        RGTextureDesc Desc{};
        TextureHandle ImportedTexture{};
        bool Imported = false;
    };

    struct CompiledPass
    {
        PassRef Ref = RG_INVALID_PASS;
        std::string Name;
        std::vector<RGTextureHandle> Reads;
        std::vector<RGTextureHandle> Writes;
        uint32_t SortedIndex = 0;
    };

} // namespace CHEngine
