#pragma once

#include <Core.h>
#include <Render/UniformBlocks.h>
#include <glm/glm.hpp>
#include <memory>
#include <string>

#include "Render/Handles.h"
#include "Render/Core/RenderTypes.h"

namespace CHEngine {
/// Базовый материал ассета: только свои поля, без MaterialInstance.
class CHENGINE_API Material
{
public:
    explicit Material(ShaderHandle handler);
    ~Material();

    Material(const Material&) = delete;
    Material& operator=(const Material&) = delete;

    ShaderHandle ShaderHandler;

    TextureHandle DiffuseMap{};
    std::string   DiffuseMapPath;

    TextureHandle SpecularMap{};
    std::string   SpecularMapPath;

    TextureHandle NormalMap{};
    std::string   NormalMapPath;

    TextureHandle ORMmap{};
    std::string   ORMmapPath;

    TextureHandle EmissiveMap{};
    std::string   EmissiveMapPath;

    float               Shininess     = 32.0f;
    float               SpecularScale = 1.0f;
    float               Roughness     = 0.5f;
    float               Metallic      = 0.0f;
    float               AO            = 1.0f;
    glm::vec3           EmissiveColor{ 0.0f };
    MaterialParamsStore MaterialFlags = 0;

    void FillUBOMaterial(UBOMaterial& out) const;

    ShaderHandle GetShaderHandle() const;
};

/// Экземпляр: база + переопределения; слияние и биндинг — здесь.
class CHENGINE_API MaterialInstance
{
public:
    explicit MaterialInstance(Ref<Material> base);
    ~MaterialInstance();

    MaterialInstance(const MaterialInstance&) = delete;
    MaterialInstance& operator=(const MaterialInstance&) = delete;

    static Ref<MaterialInstance> FromBase(Ref<Material> base);

    // Returns a new instance sharing the same base Material but with independent
    // overrides (scalar params copied; texture handles are NOT duplicated — they
    // fall through to the base material until explicitly set on the clone).
    Ref<MaterialInstance> Clone() const;

    Ref<Material> GetMaterial() const;

    Ref<Material> m_Material;

    TextureHandle DiffuseMap{};
    std::string   DiffuseMapPath;

    TextureHandle SpecularMap{};
    std::string   SpecularMapPath;

    TextureHandle NormalMap{};
    std::string   NormalMapPath;

    TextureHandle ORMmap{};
    std::string   ORMmapPath;

    TextureHandle EmissiveMap{};
    std::string   EmissiveMapPath;

    float Shininess     = 32.0f;
    float SpecularScale = 1.0f;
    float Roughness     = 0.5f;
    float Metallic      = 0.0f;
    float AO            = 1.0f;
    glm::vec3 EmissiveColor{ 0.0f };

    void ResolveTextures(TextureHandle& outDiffuse, TextureHandle& outSpecular) const;
    void ResolveAllTextures(TextureHandle& outDiffuse, TextureHandle& outSpecular,
                            TextureHandle& outNormal, TextureHandle& outORM,
                            TextureHandle& outEmissive) const;

    std::string EffectiveDiffuseMapPath() const;
    std::string EffectiveSpecularMapPath() const;
    std::string EffectiveNormalMapPath() const;
    std::string EffectiveORMmapPath() const;
    std::string EffectiveEmissiveMapPath() const;

    void FillUBOMaterial(UBOMaterial& out) const;

    void ApplyMaterial() const;
};

} // namespace CHEngine
