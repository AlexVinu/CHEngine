#pragma once

#include "EditorWorldContext.h"

#include <CHEngine.h>

#include <ImGuizmo.h>

#include <string>
#include <vector>

namespace Sandbox {

class CommandStack;
class EditorCameraController;
class EditorViewport;

/// Host surface for editor UI panels: state accessors and actions (implemented by SceneViewLayerHost).
class EditorUiHost
{
public:
    virtual ~EditorUiHost() = default;

    virtual EditorWorldContext& GetActiveSceneSession() = 0;
    virtual CommandStack& GetCommandStack() = 0;
    virtual EditorCameraController& GetEditorCameraController() = 0;
    virtual EditorViewport& GetEditorViewport() = 0;

    virtual ImGuizmo::OPERATION& GetGizmoOperation() = 0;
    virtual ImGuizmo::MODE& GetGizmoMode() = 0;
    virtual bool& GetLocalMode() = 0;
    virtual bool& GetShowProfiler() = 0;
    virtual bool& GetShowUVEditor() = 0;

    virtual std::vector<EditorWorldContext>& GetSceneSessions() = 0;
    virtual size_t GetActiveSessionIndex() const = 0;
    virtual void SetActiveSessionIndex(size_t session_index) = 0;
    virtual void AddSceneSession() = 0;
    /// Close session at index. Last session cannot be closed (no-op).
    virtual void CloseSceneSession(size_t session_index) = 0;
    /// Open a scene file in a new tab (or switch to existing tab bound to that file).
    /// `relOrAbsPath` may be relative to project root or absolute.
    virtual void OpenSceneFile(const std::string& relOrAbsPath) = 0;

    virtual CHEngine::Transform& GetTransformBeforeDrag() = 0;

    virtual void RequestUndo() = 0;

    virtual void EnterPlayMode() = 0;
    virtual void EnterPauseMode() = 0;
    virtual void ResumeFromPause() = 0;
    virtual void StopPlayMode() = 0;

    virtual void SaveScene() = 0;
    virtual void OpenSceneDialog() = 0;
    virtual void AutoSaveForRestart() = 0;

    // ── Scene Browser ────────────────────────────────────────────────────────
    /// Toggle the Scene Browser side panel.
    virtual void ToggleSceneBrowser() = 0;
    /// Create a new empty `Untitled_N.chscene` under <project>/Scenes/ and open it.
    virtual void NewSceneFile() = 0;
    /// Delete a scene file from disk. Closes any open tabs bound to it. `rel` is relative to project root.
    virtual void DeleteSceneFile(const std::string& rel) = 0;
    /// Rename a scene file on disk and update SceneRelPath of any open tabs bound to it.
    virtual void RenameSceneFile(const std::string& oldRel, const std::string& newName) = 0;
    /// Set this scene file as the project's startup (Sessions[0]).
    virtual void SetStartupSceneFile(const std::string& rel) = 0;

    virtual void ImportModel(const std::string& filepath) = 0;

    virtual void ApplyOrbit() = 0;
    virtual void SetViewPreset(float yaw_degrees, float pitch_degrees) = 0;
    virtual void SetViewportFov(float fov_degrees) = 0;
    virtual void FocusOnSelected() = 0;
    virtual void ResetViewportCamera() = 0;

    virtual void AddDirectionalLight() = 0;
    virtual void AddPointLight() = 0;
    virtual void AddSpotLight() = 0;
    virtual void AddCubePrimitive() = 0;
    virtual void AddSpherePrimitive() = 0;
    virtual void AddCameraEntity() = 0;
    virtual void AddEmptyEntity() = 0;
    virtual void SetSelection(CHEngine::EntityHandle handle) = 0;
    virtual void DestroyEntityByUuid(const CHEngine::UUID& object_id) = 0;

    virtual void OnRendererApiSelected(CHEngine::ERenderAPI api) = 0;
    virtual void ToggleUiTheme() = 0;

    // ── Project ──────────────────────────────────────────────────────────────
    virtual void OnProjectChanged() = 0;

    virtual void ApplyDiffuseTextureToSelectedSubmesh(size_t submesh_index, const std::string& filepath) = 0;
    virtual void ClearDiffuseTextureOnSelectedSubmesh(size_t submesh_index) = 0;
    virtual void ApplySpecularTextureToSelectedSubmesh(size_t submesh_index, const std::string& filepath) = 0;
    virtual void ClearSpecularTextureOnSelectedSubmesh(size_t submesh_index) = 0;
};

} // namespace Sandbox
