#pragma once

#include "SceneSession.h"
#include "SetTransformCommand.h"

#include <CHEngine.h>
#include <ImGuizmo.h>
#include <nlohmann/json.hpp>

/// Per-session editor state: scene/world/camera plus undo, gizmo UI mode, and play snapshot.
struct EditorWorldContext : public SceneSession
{
    /// Ensures base SceneSession (ViewportCamera, RuntimeWorld, etc.) is always initialized.
    EditorWorldContext();
    EditorWorldContext(const EditorWorldContext&) = delete;
    EditorWorldContext& operator=(const EditorWorldContext&) = delete;
    EditorWorldContext(EditorWorldContext&&) = default;
    EditorWorldContext& operator=(EditorWorldContext&&) = default;

    Sandbox::CommandStack m_CommandStack{};
    CHEngine::Transform m_TransformBeforeDrag{};
    CHEngine::EntityHandle m_EditorCameraEntity{};

    nlohmann::json m_SceneSnapshot{};
    bool m_HasSceneSnapshot = false;

    ImGuizmo::OPERATION m_GizmoOperation = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE m_GizmoMode = ImGuizmo::WORLD;
    bool m_LocalMode = false;
    bool m_ShowProfiler = false;
    bool m_ResetLayout = false;
    float m_ContentBrowserHeight = 200.0f;
    float m_StepDt = 1.0f / 60.0f;

    void UpdateSimulation(CHEngine::Timestep dt);
};
