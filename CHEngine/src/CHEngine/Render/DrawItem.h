#pragma once

#include <Render/Handles.h>
#include <Render/Core/RenderTypes.h>
#include <Render/UniformBlocks.h>
#include <CheStl/MemoryTypes.h>
#include <glm/glm.hpp>
#include <memory>

namespace CHEngine {

    class MaterialInstance;

    // A fully resolved, ECS-agnostic draw command.
    // Built by RenderSystem::BuildDrawList() before the render graph is constructed.
    // Future passes consume this list without touching Scene or any ECS type.
    //
    // Note: vertex layout is implicit (the standard mesh layout) until Phase 3
    // introduces explicit PipelineHandle, at which point layout moves into PipelineDesc.
    struct DrawItem
    {
        ShaderHandle          shader;        // resolved from MaterialInstance
        BufferHandle          vertexBuffer;  // mesh VBO
        BufferHandle          indexBuffer;   // mesh IBO
        IndexFormat           indexFormat = IndexFormat::UInt32;
        uint32_t              indexCount  = 0;
        glm::mat4             modelMatrix; // pre-computed model transform
        UBOObject             object;      // Transform, NormalMatrix, Color, Selected
        Ref<MaterialInstance> material;    // kept alive for ApplyMaterial()
    };

} // namespace CHEngine
