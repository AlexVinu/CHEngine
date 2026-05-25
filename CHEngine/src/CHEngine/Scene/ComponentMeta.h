#pragma once

#include <Core.h>

namespace CHEngine {

// Registers all CopyableSceneComponents with entt::meta.
// Must be called once at Application startup, before any World is created.
// Registers: clone_to function (for entity transfer and scene cloning).
CHENGINE_API void RegisterAllComponents();

} // namespace CHEngine
