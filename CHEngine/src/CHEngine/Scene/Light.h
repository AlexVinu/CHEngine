#pragma once

#include <Core.h>
#include <glm/glm.hpp>

namespace CHEngine {

// ─── Типы источников света ────────────────────────────────────────────────
enum class LightType : int
{
    None        = -1,   // объект не является источником света
    Directional =  0,   // направленный (солнце)
    Point       =  1,   // точечный (лампа)
    Spot        =  2    // прожектор (конус)
};

// ─── Параметры источника света ────────────────────────────────────────────
// Направление (Directional/Spot) берётся из Transform.Rotation объекта.
// Позиция (Point/Spot) берётся из Transform.Position объекта.
struct CHENGINE_API Light
{
    LightType Type      = LightType::None;
    glm::vec3 Color     = { 1.0f, 1.0f, 1.0f };
    float     Intensity = 1.0f;

    // Point / Spot
    float Range = 10.0f;

    // Spot (углы в градусах)
    float InnerCone = 12.5f;
    float OuterCone = 17.5f;
};

} // namespace CHEngine
