#pragma once

#include "EditorWorldContext.h"
#include "EditorViewport.h"
#include "EditorCameraController.h"
#include "GizmoSystem.h"
#include "TilingManager.h"
#include "ProjectManager.h"
#include "ScriptEditorPanel.h"
#include "SceneBrowserPanel.h"
#include "ExportPanel.h"

#include <CheStl/MemoryTypes.h>
#include <CHEngine/World/WorldsList.h>

#include <cstddef>

namespace Sandbox {

struct EditorContext final
{
    EditorContext() = default;

    EditorContext(const EditorContext&)            = delete;
    EditorContext& operator=(const EditorContext&) = delete;
    EditorContext(EditorContext&&)                 = delete;
    EditorContext& operator=(EditorContext&&)      = delete;

    // Worlds and Projects are owned by SandboxApp; this struct holds shared refs.
    Ref<CHEngine::WorldsList>   Worlds;
    size_t                      ActiveIndex = 0;
    Ref<ProjectManager>         Projects;

    EditorViewport         Viewport;
    EditorCameraController CameraController;
    GizmoSystem            Gizmo;
    TilingManager          Tiling{ "tiling_layout.txt" };

    // Panels accessed by command logic outside SceneViewLayer.
    ScriptEditorPanel  ScriptEditor;
    SceneBrowserPanel  SceneBrowser;
    ExportPanel        Export;

    EditorWorldContext* ActiveCtx() const
    {
        if (!Worlds || ActiveIndex >= Worlds->Size())
            return nullptr;
        return static_cast<EditorWorldContext*>(Worlds->GetForIndex(ActiveIndex));
    }

    /// Switch the active session: deactivates the old one, activates the new one.
    void SetActiveIndex(size_t index)
    {
        CHE_ASSERT(Worlds && index < Worlds->Size(), "Invalid session index");

        if (ActiveIndex < Worlds->Size())
            ActiveCtx()->UpdateState(std::nullopt, false);

        ActiveIndex = index;
        ActiveCtx()->UpdateState(std::nullopt, true);
    }
};

} // namespace Sandbox
