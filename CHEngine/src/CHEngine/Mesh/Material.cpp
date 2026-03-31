#include "chepch.h"

#include "Material.h"

#include <Render/IShader.h>
#include <Render/ITexture.h>
#include <Render/UniformBlocks.h>

#include "CHEngine/Render/RenderFacade.h"

namespace CHEngine
{

Material::Material(ShaderHandle handler)
    : ShaderHandler(handler)
{
    CHE_CORE_ASSERT(handler.IsValid(), "Material — ShaderHandle must be valid");
}

void Material::FillUBOMaterial(UBOMaterial& out) const
{
    out.UseTexture     = DiffuseMap.IsValid() ? 1 : 0;
    out.UseSpecularMap = SpecularMap.IsValid() ? 1 : 0;
    out.Shininess      = Shininess;
    out.MaterialFlags  = MaterialFlags;
    out.SpecularScale  = SpecularScale;
    out._pad0 = out._pad1 = out._pad2 = 0.0f;
}

MaterialInstance::MaterialInstance(std::shared_ptr<Material> base)
    : Base(std::move(base))
{
    CHE_CORE_ASSERT(Base && Base->ShaderHandler.IsValid(), "MaterialInstance — base material with valid shader required");
    Shininess     = Base->Shininess;
    SpecularScale = Base->SpecularScale;
}

std::shared_ptr<MaterialInstance> MaterialInstance::FromBase(std::shared_ptr<Material> base)
{
    CHE_CORE_ASSERT(base && base->ShaderHandler.IsValid(), "FromBase — base material with valid shader required");
    return std::make_shared<MaterialInstance>(std::move(base));
}

ShaderHandle MaterialInstance::GetShaderHandle() const
{
    return Base->ShaderHandler;
}

void MaterialInstance::ResolveTextures(TextureHandle& outDiffuse, TextureHandle& outSpecular) const
{
    outDiffuse  = DiffuseMap.IsValid() ? DiffuseMap : Base->DiffuseMap;
    outSpecular = SpecularMap.IsValid() ? SpecularMap : Base->SpecularMap;
}

std::string MaterialInstance::EffectiveDiffuseMapPath() const
{
    if (!DiffuseMapPath.empty())
        return DiffuseMapPath;
    if (Base && !Base->DiffuseMapPath.empty())
        return Base->DiffuseMapPath;
    return {};
}

std::string MaterialInstance::EffectiveSpecularMapPath() const
{
    if (!SpecularMapPath.empty())
        return SpecularMapPath;
    if (Base && !Base->SpecularMapPath.empty())
        return Base->SpecularMapPath;
    return {};
}

void MaterialInstance::FillUBOMaterial(UBOMaterial& out) const
{
    Base->FillUBOMaterial(out);

    TextureHandle d, s;
    ResolveTextures(d, s);
    out.UseTexture     = d.IsValid() ? 1 : 0;
    out.UseSpecularMap = s.IsValid() ? 1 : 0;
    out.Shininess      = Shininess;
    out.SpecularScale  = SpecularScale;
}

void MaterialInstance::ApplyMaterial() const
{
    IShader* shader = RenderFacade::GetShader(Base->ShaderHandler);
    if (!shader)
        return;

    UBOMaterial materialUBO;
    FillUBOMaterial(materialUBO);

    TextureHandle d, s;
    ResolveTextures(d, s);

    ITexture* diffTex = d.IsValid() ? RenderFacade::GetTexture(d) : nullptr;
    ITexture* specTex = s.IsValid() ? RenderFacade::GetTexture(s) : nullptr;

    if (diffTex)
    {
        diffTex->Bind(0);
        shader->SetInt(String("u_DiffuseTexture"), 0);
    }

    if (specTex)
    {
        specTex->Bind(1);
        shader->SetInt(String("u_SpecularMap"), 1);
    }

    shader->SetUniformBlock(EUniformBlock::Material, &materialUBO, sizeof(materialUBO));

    if (diffTex)
        diffTex->Unbind();
    if (specTex)
        specTex->Unbind();
}

} // namespace CHEngine
