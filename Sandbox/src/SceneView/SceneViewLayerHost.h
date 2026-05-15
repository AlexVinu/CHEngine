#pragma once

#include "EditorWorldContext.h"

#include <CHEngine.h>

#include <ImGuizmo.h>

#include <string>
#include <vector>

class SceneViewLayer;
class ProjectManager;

namespace Sandbox {

class CommandStack;
class EditorCameraController;
class EditorViewport;

class SceneViewLayerHost
{
public:
    explicit SceneViewLayerHost(SceneViewLayer& layer);

    Ref<EditorWorldContext> GetActiveSceneSession() ;
    Ref<ProjectManager> GetProjectManager() ;
    Sandbox::CommandStack& GetCommandStack() ;
    Sandbox::EditorCameraController& GetEditorCameraController() ;
    Sandbox::EditorViewport& GetEditorViewport() ;

    ImGuizmo::OPERATION& GetGizmoOperation();
    ImGuizmo::MODE& GetGizmoMode();
    bool& GetLocalMode();
    bool& GetShowProfiler();

    Ref<std::vector<Ref<EditorWorldContext>>> GetSceneSessions() ;
    size_t GetActiveSessionIndex() const ;
    void SetActiveSessionIndex(size_t session_index) ;
    void AddSceneSession() ;
    void CloseSceneSession(size_t session_index) ;
    void OpenSceneFile(const std::string& relOrAbsPath) ;

    CHEngine::Transform& GetTransformBeforeDrag() ;

    void RequestUndo() ;

    void EnterPlayMode() ;
    void EnterPauseMode() ;
    void ResumeFromPause() ;
    void StopPlayMode() ;

    void SaveScene() ;
    void OpenSceneDialog() ;
    void AutoSaveForRestart() ;

    void ToggleSceneBrowser() ;
    void NewSceneFile() ;
    void DeleteSceneFile(const std::string& rel) ;
    void RenameSceneFile(const std::string& oldRel, const std::string& newName) ;
    void SetStartupSceneFile(const std::string& rel) ;

    void ImportModel(const std::string& filepath) ;

    void ApplyOrbit() ;
    void SetViewPreset(float yaw_degrees, float pitch_degrees) ;
    void SetViewportFov(float fov_degrees) ;
    void FocusOnSelected() ;
    void ResetViewportCamera() ;

    void AddDirectionalLight();
    void AddPointLight();
    void AddSpotLight();
    void AddCubePrimitive();
    void AddSpherePrimitive();
    void AddCameraEntity();
    void AddEmptyEntity();
    void SetSelection(CHEngine::EntityHandle handle);
    void DestroyEntityByUuid(const CHEngine::UUID& object_id);

    void OnRendererApiSelected(CHEngine::ERenderAPI api) ;
    void ToggleUiTheme() ;
    void OnProjectChanged() ;

    void OpenScriptInEditor(const std::string& path) ;
    void CreateAndAttachScript(CHEngine::EntityHandle handle, const std::string& entityName) ;

    // Global AI overlay commands
    void ApplyLayoutPreset(const std::string& presetName) ;
    void SelectEntityByName(const std::string& name) ;
    void OpenScriptForEntity(const std::string& entityName) ;
    void CreateAndAttachScriptToEntityByName(const std::string& entityName) ;
    // Set position on currently selected entity (call right after Add*Primitive)
    void SetSelectedEntityPosition(float x, float y, float z) ;
    // Create script with specific Lua content and attach to currently selected entity
    void CreateAndAttachScriptWithContent(const std::string& entityName,
                                          const std::string& luaContent) ;

    void ApplyDiffuseTextureToSelectedSubmesh(size_t submesh_index, const std::string& filepath) ;
    void ClearDiffuseTextureOnSelectedSubmesh(size_t submesh_index) ;
    void ApplySpecularTextureToSelectedSubmesh(size_t submesh_index, const std::string& filepath) ;
    void ClearSpecularTextureOnSelectedSubmesh(size_t submesh_index) ;

private:
    SceneViewLayer& m_Layer;
};

} // namespace Sandbox
