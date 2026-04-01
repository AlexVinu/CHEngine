#pragma once

#include <Core.h>
#include <Render/UniformBlocks.h>
#include <memory>
#include <string>

#include "CHEngine/Render/RenderResourceManager.h"

namespace CHEngine {
/// Базовый материал ассета: только свои поля, без MaterialInstance.
class CHENGINE_API Material
{
public:
    explicit Material(ShaderHandle handler);

    ShaderHandle ShaderHandler;

    TextureHandle DiffuseMap{};
    std::string   DiffuseMapPath;

    TextureHandle SpecularMap{};
    std::string   SpecularMapPath;

    float               Shininess     = 32.0f;
    float               SpecularScale = 1.0f;
    MaterialParamsStore MaterialFlags = 0;

    void FillUBOMaterial(UBOMaterial& out) const;
};

/// Экземпляр: база + переопределения; слияние и биндинг — здесь.
class CHENGINE_API MaterialInstance
{
public:
    explicit MaterialInstance(Ref<Material> base);

    static Ref<MaterialInstance> FromBase(Ref<Material> base);

    ShaderHandle GetShaderHandle() const;

    Ref<Material> Base;

    TextureHandle DiffuseMap{};
    std::string   DiffuseMapPath;

    TextureHandle SpecularMap{};
    std::string   SpecularMapPath;

    float Shininess     = 32.0f;
    float SpecularScale = 1.0f;

    void ResolveTextures(TextureHandle& outDiffuse, TextureHandle& outSpecular) const;

    std::string EffectiveDiffuseMapPath() const;
    std::string EffectiveSpecularMapPath() const;

    void FillUBOMaterial(UBOMaterial& out) const;

    void ApplyMaterial() const;
};

} // namespace CHEngine
