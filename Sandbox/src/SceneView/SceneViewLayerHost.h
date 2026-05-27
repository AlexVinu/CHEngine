#pragma once

#include "EditorWorldContext.h"

#include <CHEngine.h>

#include <ImGuizmo.h>

#include <string>
#include <vector>
#include <cstddef>

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

    EditorWorldContext* GetActiveSceneSession() ;
    Ref<ProjectManager> GetProjectManager() ;
    Sandbox::CommandStack& GetCommandStack() ;
    Sandbox::EditorCameraController& GetEditorCameraController() ;
    Sandbox::EditorViewport& GetEditorViewport() ;

    ImGuizmo::OPERATION& GetGizmoOperation();
    ImGuizmo::MODE& GetGizmoMode();
    bool& GetLocalMode();
    bool& GetShowProfiler();

    size_t GetActiveSessionIndex() const ;
    void SetActiveSessionIndex(size_t session_index) ;
    std::vector<EditorWorldContext*> GetSceneSessions() const ;
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
    void OpenImportModelDialog() ;

    void ApplyOrbit() ;
    void SetViewPreset(float yaw_degrees, float pitch_degrees) ;
    void FocusOnSelected() ;
    void ResetViewportCamera() ;

    void AddDirectionalLight();
    void AddPointLight();
    void AddSpotLight();
    void AddCubePrimitive();
    void AddSpherePrimitive();
    void AddCameraEntity();
    void AddEmptyEntity();

    // UI entities. Canvas is a separate entity (overlay or world); elements
    // attach to a canvas via UIRectTransformComponent::CanvasRef.
    void AddUIOverlayCanvas();
    void AddUIWorldCanvas();

    void AddUIPanel(const CHEngine::UUID&);
    void AddUIText(const CHEngine::UUID&);
    void AddUIButton(const CHEngine::UUID&);
    void AddUIImage(const CHEngine::UUID&);
    void AddUISlider(const CHEngine::UUID&);
    void SetSelection(CHEngine::EntityHandle handle);
    void DestroyEntityByUuid(const CHEngine::UUID& object_id);

    void OnRendererApiSelected(CHEngine::ERenderAPI api) ;
    void ToggleUiTheme() ;
    void OnProjectChanged() ;

    void OpenScriptInEditor(const std::string& path) ;
    void CreateAndAttachScript(CHEngine::EntityHandle handle, const std::string& entityName) ;
    void CreateAndAttachWorldScript() ;
    void CreateAndAttachWorldScriptWithContent(const std::string& luaContent) ;

    // ── Global AI overlay commands ────────────────────────────────────────────
    void ApplyLayoutPreset(const std::string& presetName) ;
    void SelectEntityByName(const std::string& name) ;
    void FocusOnEntityByName(const std::string& name) ;
    void OpenScriptForEntity(const std::string& entityName) ;
    void CreateAndAttachScriptToEntityByName(const std::string& entityName) ;
    void CreateAndAttachScriptWithContent(const std::string& entityName,
                                          const std::string& luaContent) ;

    // Transform on selected entity (use right after Add*Primitive)
    void SetSelectedEntityPosition(float x, float y, float z) ;
    void SetSelectedEntityRotation(float x, float y, float z) ;
    void SetSelectedEntityScale(float x, float y, float z) ;
    void RenameSelectedEntity(const std::string& newName) ;

    // Transform / properties on named entity
    void SetEntityPositionByName(const std::string& name, float x, float y, float z) ;
    void SetEntityRotationByName(const std::string& name, float x, float y, float z) ;
    void SetEntityScaleByName(const std::string& name, float x, float y, float z) ;
    void SetEntityColorByName(const std::string& name, float r, float g, float b, float a) ;
    void SetEntityVisibleByName(const std::string& name, bool visible) ;
    void RenameEntityByName(const std::string& oldName, const std::string& newName) ;
    void DeleteEntityByName(const std::string& name) ;
    void SetEntityLightByName(const std::string& name,
                               const std::string& lightType,
                               float r, float g, float b, float intensity) ;

    // Editor state
    void SetViewportFovValue(float fov) ;   // alias for SetViewportFov

    // Scene introspection for AI context
    std::string GetSceneContextString() ;

    // Export panel
    void OpenExportPanel() ;

    void ApplyDiffuseTextureToSelectedSubmesh(size_t submesh_index, const std::string& filepath) ;
    void ClearDiffuseTextureOnSelectedSubmesh(size_t submesh_index) ;
    void ApplySpecularTextureToSelectedSubmesh(size_t submesh_index, const std::string& filepath) ;
    void ClearSpecularTextureOnSelectedSubmesh(size_t submesh_index) ;

private:
    SceneViewLayer& m_Layer;
};

} // namespace Sandbox
