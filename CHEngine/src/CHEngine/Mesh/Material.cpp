#include "chepch.h"

#include "Material.h"

#include <Render/UniformBlocks.h>

#include "CHEngine/Application.h"

namespace CHEngine
{

Material::Material(ShaderHandle handler)
    : ShaderHandler(handler)
{
    CHE_CORE_ASSERT(handler.IsValid(), "Material — ShaderHandle must be valid");
}

Material::~Material()
{
    if (DiffuseMap.IsValid())
        Application::Get().Render().DestroyTexture(DiffuseMap);
    if (SpecularMap.IsValid())
        Application::Get().Render().DestroyTexture(SpecularMap);
    if (NormalMap.IsValid())
        Application::Get().Render().DestroyTexture(NormalMap);
    if (ORMmap.IsValid())
        Application::Get().Render().DestroyTexture(ORMmap);
    if (EmissiveMap.IsValid())
        Application::Get().Render().DestroyTexture(EmissiveMap);
}

void Material::FillUBOMaterial(UBOMaterial& out) const
{
    out.UseTexture     = DiffuseMap.IsValid() ? 1 : 0;
    out.UseSpecularMap = SpecularMap.IsValid() ? 1 : 0;
    out.Shininess      = Shininess;
    out.MaterialFlags  = MaterialFlags;
    out.SpecularScale  = SpecularScale;
    out.Roughness      = Roughness;
    out.Metallic       = Metallic;
    out.AO             = AO;
}

ShaderHandle Material::GetShaderHandle() const
{
    return ShaderHandler;
}

MaterialInstance::MaterialInstance(Ref<Material> base)
    : m_Material(std::move(base))
{
    CHE_CORE_ASSERT(m_Material && m_Material->ShaderHandler.IsValid(), "MaterialInstance — base material with valid shader required");
    Shininess     = m_Material->Shininess;
    SpecularScale = m_Material->SpecularScale;
    Roughness     = m_Material->Roughness;
    Metallic      = m_Material->Metallic;
    AO            = m_Material->AO;
    EmissiveColor = m_Material->EmissiveColor;
}

MaterialInstance::~MaterialInstance()
{
    if (DiffuseMap.IsValid())
        Application::Get().Render().DestroyTexture(DiffuseMap);
    if (SpecularMap.IsValid())
        Application::Get().Render().DestroyTexture(SpecularMap);
    if (NormalMap.IsValid())
        Application::Get().Render().DestroyTexture(NormalMap);
    if (ORMmap.IsValid())
        Application::Get().Render().DestroyTexture(ORMmap);
    if (EmissiveMap.IsValid())
        Application::Get().Render().DestroyTexture(EmissiveMap);
    DiffuseMap = {};
    SpecularMap = {};
    NormalMap = {};
    ORMmap = {};
    EmissiveMap = {};
}

Ref<MaterialInstance> MaterialInstance::FromBase(Ref<Material> base)
{
    CHE_CORE_ASSERT(base && base->ShaderHandler.IsValid(), "FromBase — base material with valid shader required");
    return std::make_shared<MaterialInstance>(std::move(base));
}

Ref<MaterialInstance> MaterialInstance::Clone() const
{
    auto inst = std::make_shared<MaterialInstance>(m_Material);
    inst->DiffuseMapPath  = DiffuseMapPath;
    inst->SpecularMapPath = SpecularMapPath;
    inst->NormalMapPath   = NormalMapPath;
    inst->ORMmapPath      = ORMmapPath;
    inst->EmissiveMapPath = EmissiveMapPath;
    inst->Shininess       = Shininess;
    inst->SpecularScale   = SpecularScale;
    inst->Roughness       = Roughness;
    inst->Metallic        = Metallic;
    inst->AO              = AO;
    inst->EmissiveColor   = EmissiveColor;
    return inst;
}

Ref<Material> MaterialInstance::GetMaterial() const
{
    return m_Material;
}

void MaterialInstance::ResolveTextures(TextureHandle& outDiffuse, TextureHandle& outSpecular) const
{
    outDiffuse  = DiffuseMap.IsValid() ? DiffuseMap : m_Material->DiffuseMap;
    outSpecular = SpecularMap.IsValid() ? SpecularMap : m_Material->SpecularMap;
}

void MaterialInstance::ResolveAllTextures(TextureHandle& outDiffuse, TextureHandle& outSpecular,
                                          TextureHandle& outNormal, TextureHandle& outORM,
                                          TextureHandle& outEmissive) const
{
    outDiffuse  = DiffuseMap.IsValid()  ? DiffuseMap  : m_Material->DiffuseMap;
    outSpecular = SpecularMap.IsValid() ? SpecularMap : m_Material->SpecularMap;
    outNormal   = NormalMap.IsValid()   ? NormalMap   : m_Material->NormalMap;
    outORM      = ORMmap.IsValid()      ? ORMmap      : m_Material->ORMmap;
    outEmissive = EmissiveMap.IsValid() ? EmissiveMap : m_Material->EmissiveMap;
}

std::string MaterialInstance::EffectiveDiffuseMapPath() const
{
    if (!DiffuseMapPath.empty())
        return DiffuseMapPath;
    if (m_Material && !m_Material->DiffuseMapPath.empty())
        return m_Material->DiffuseMapPath;
    return {};
}

std::string MaterialInstance::EffectiveSpecularMapPath() const
{
    if (!SpecularMapPath.empty())
        return SpecularMapPath;
    if (m_Material && !m_Material->SpecularMapPath.empty())
        return m_Material->SpecularMapPath;
    return {};
}

std::string MaterialInstance::EffectiveNormalMapPath() const
{
    if (!NormalMapPath.empty())
        return NormalMapPath;
    if (m_Material && !m_Material->NormalMapPath.empty())
        return m_Material->NormalMapPath;
    return {};
}

std::string MaterialInstance::EffectiveORMmapPath() const
{
    if (!ORMmapPath.empty())
        return ORMmapPath;
    if (m_Material && !m_Material->ORMmapPath.empty())
        return m_Material->ORMmapPath;
    return {};
}

std::string MaterialInstance::EffectiveEmissiveMapPath() const
{
    if (!EmissiveMapPath.empty())
        return EmissiveMapPath;
    if (m_Material && !m_Material->EmissiveMapPath.empty())
        return m_Material->EmissiveMapPath;
    return {};
}

void MaterialInstance::FillUBOMaterial(UBOMaterial& out) const
{
    m_Material->FillUBOMaterial(out);

    TextureHandle d, s;
    ResolveTextures(d, s);
    out.UseTexture     = d.IsValid() ? 1 : 0;
    out.UseSpecularMap = s.IsValid() ? 1 : 0;
    out.Shininess      = Shininess;
    out.SpecularScale  = SpecularScale;
    out.Roughness      = Roughness;
    out.Metallic       = Metallic;
    out.AO             = AO;
}

void MaterialInstance::ApplyMaterial() const
{
    // TODO (Phase 6): material binding moves into the declarative MainColorPass:
    //   - PassDesc lists (shader, vertexBuffer, indexBuffer, uniformBuffers, textureBindings)
    //   - Backend FG binds them when executing the pass.
    // For now this is a no-op so the engine boots. UBOMaterial is still computed
    // through FillUBOMaterial() for any caller that wants the data.
}

} // namespace CHEngine
