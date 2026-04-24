#pragma once

#include "SceneSession.h"
#include "SetTransformCommand.h"
#include "EditorCameraState.h"

#include <CHEngine.h>
#include <ImGuizmo.h>

/// Per-session editor state: scene/world/camera plus undo, gizmo UI mode, and play snapshot.
struct EditorWorldContext : public SceneSession
{
    /// Ensures base SceneSession (ViewportCamera, RuntimeWorld, etc.) is always initialized.
    EditorWorldContext();
    EditorWorldContext(const EditorWorldContext&) = delete;
    EditorWorldContext& operator=(const EditorWorldContext&) = delete;
    EditorWorldContext(EditorWorldContext&&) = default;
    EditorWorldContext& operator=(EditorWorldContext&&) = default;

    Sandbox::CommandStack CommandStack{};
    CHEngine::Transform TransformBeforeDrag{};

    ImGuizmo::OPERATION GizmoOperation = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE GizmoMode = ImGuizmo::WORLD;
    bool LocalMode = false;
    bool ShowProfiler = false;
    bool ResetLayout = false;
    bool IsActive = false;
    float ContentBrowserHeight = 200.0f;
    float StepDt = 1.0f / 60.0f;
    Sandbox::EditorCameraState EditorCameraState{};

    void Update(CHEngine::Timestep dt);
    void ActivateActiveScene();
    void ActivateEditorScene();
};
