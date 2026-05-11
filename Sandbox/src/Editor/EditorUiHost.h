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

    virtual CHEngine::Transform& GetTransformBeforeDrag() = 0;

    virtual void RequestUndo() = 0;

    virtual void EnterPlayMode() = 0;
    virtual void EnterPauseMode() = 0;
    virtual void ResumeFromPause() = 0;
    virtual void StopPlayMode() = 0;

    virtual void SaveScene() = 0;
    virtual void OpenSceneDialog() = 0;
    virtual void AutoSaveForRestart() = 0;

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
    virtual void AddEmptyEntity() = 0;
    virtual void SetSelection(CHEngine::EntityHandle handle) = 0;
    virtual void DestroyEntityByUuid(const CHEngine::UUID& object_id) = 0;

    virtual void OnRendererApiSelected(CHEngine::ERenderAPI api) = 0;
    virtual void ToggleUiTheme() = 0;

    virtual void ApplyDiffuseTextureToSelectedSubmesh(size_t submesh_index, const std::string& filepath) = 0;
    virtual void ClearDiffuseTextureOnSelectedSubmesh(size_t submesh_index) = 0;
    virtual void ApplySpecularTextureToSelectedSubmesh(size_t submesh_index, const std::string& filepath) = 0;
    virtual void ClearSpecularTextureOnSelectedSubmesh(size_t submesh_index) = 0;
};

} // namespace Sandbox
