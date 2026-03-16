#pragma once

#include <Core.h>
#include <string>

#include "CHEngine/Render/RenderResourceManager.h"

namespace CHEngine {

// ─── Материал меша (Blinn-Phong) ──────────────────────────────────────────────
// Диффузная и specular текстуры + параметры освещения.
// Позиция и масштаб берутся из Transform объекта-владельца.
// Если текстура не задана (Invalid handle) — используется SceneObject::Color.
struct CHENGINE_API Material
{
    // Диффузная (albedo) текстура — умножается на SceneObject::Color
    TextureHandle DiffuseMap;
    std::string   DiffuseMapPath;  // путь к файлу для сериализации / перезагрузки

    // Specular текстура (R-канал = интенсивность блика)
    TextureHandle SpecularMap;
    std::string   SpecularMapPath;

    // Параметр глянцевости Blinn-Phong (степень в pow())
    float Shininess = 32.0f;
};

} // namespace CHEngine
