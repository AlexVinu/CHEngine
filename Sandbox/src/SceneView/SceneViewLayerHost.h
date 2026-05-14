#pragma once

#include "EditorUiHost.h"

class SceneViewLayer;

/// Implements EditorUiHost by forwarding into SceneViewLayer state (stack host during ImGui).
class SceneViewLayerHost final : public Sandbox::EditorUiHost
{
public:
    explicit SceneViewLayerHost(SceneViewLayer& layer);

    EditorWorldContext& GetActiveSceneSession() override;
    Sandbox::CommandStack& GetCommandStack() override;
    Sandbox::EditorCameraController& GetEditorCameraController() override;
    Sandbox::EditorViewport& GetEditorViewport() override;

    ImGuizmo::OPERATION& GetGizmoOperation() override;
    ImGuizmo::MODE& GetGizmoMode() override;
    bool& GetLocalMode() override;
    bool& GetShowProfiler() override;
    bool& GetShowUVEditor() override;

    std::vector<EditorWorldContext>& GetSceneSessions() override;
    size_t GetActiveSessionIndex() const override;
    void SetActiveSessionIndex(size_t session_index) override;
    void AddSceneSession() override;
    void CloseSceneSession(size_t session_index) override;
    void OpenSceneFile(const std::string& relOrAbsPath) override;

    CHEngine::Transform& GetTransformBeforeDrag() override;

    void RequestUndo() override;

    void EnterPlayMode() override;
    void EnterPauseMode() override;
    void ResumeFromPause() override;
    void StopPlayMode() override;

    void SaveScene() override;
    void OpenSceneDialog() override;
    void AutoSaveForRestart() override;

    void ToggleSceneBrowser() override;
    void NewSceneFile() override;
    void DeleteSceneFile(const std::string& rel) override;
    void RenameSceneFile(const std::string& oldRel, const std::string& newName) override;
    void SetStartupSceneFile(const std::string& rel) override;

    void ImportModel(const std::string& filepath) override;

    void ApplyOrbit() override;
    void SetViewPreset(float yaw_degrees, float pitch_degrees) override;
    void SetViewportFov(float fov_degrees) override;
    void FocusOnSelected() override;
    void ResetViewportCamera() override;

    void AddDirectionalLight() override;
    void AddPointLight() override;
    void AddSpotLight() override;
    void AddCubePrimitive() override;
    void AddSpherePrimitive() override;
    void AddCameraEntity() override;
    void AddEmptyEntity() override;
    void SetSelection(CHEngine::EntityHandle handle) override;
    void DestroyEntityByUuid(const CHEngine::UUID& object_id) override;

    void OnRendererApiSelected(CHEngine::ERenderAPI api) override;
    void ToggleUiTheme() override;
    void OnProjectChanged() override;

    void ApplyDiffuseTextureToSelectedSubmesh(size_t submesh_index, const std::string& filepath) override;
    void ClearDiffuseTextureOnSelectedSubmesh(size_t submesh_index) override;
    void ApplySpecularTextureToSelectedSubmesh(size_t submesh_index, const std::string& filepath) override;
    void ClearSpecularTextureOnSelectedSubmesh(size_t submesh_index) override;

private:
    SceneViewLayer& m_Layer;
};
