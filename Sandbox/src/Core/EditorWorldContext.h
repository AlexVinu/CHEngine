#pragma once

#include "SceneSession.h"
#include "SetTransformCommand.h"
#include "EditorCameraState.h"

#include <CHEngine.h>
#include <ImGuizmo.h>

#include <optional>

/// Per-session editor state: scene/world/camera plus undo, gizmo UI mode, and play snapshot.
struct EditorWorldContext final : public SceneSession
{
    /// Ensures base SceneSession (ViewportCamera, RuntimeWorld, etc.) is always initialized.
    EditorWorldContext(CHEngine::WorldsList* list);
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
    float ContentBrowserHeight = 200.0f;
    float StepDt = 1.0f / 60.0f;
    Sandbox::EditorCameraState EditorCameraState{};

    /// Relative scene file path from project root (e.g. "Scenes/Main.chscene").
    /// Empty for new unsaved scenes.
    std::string SceneRelPath{};

    void UpdateState(std::optional<SceneSession::State> new_state, std::optional<bool> is_active = std::nullopt);
    void ActivateActiveScene();
    void ActivateEditorScene();

    // ── Scene authoring commands ──────────────────────────────────────────────────
    // All ops work on EditorScene and update SelectedEntity.
    void SelectEntity(CHEngine::EntityHandle handle) { SelectedEntity = handle; }
    void DestroyEntityByUuid(const CHEngine::UUID& object_id);

    void AddDirectionalLight();
    void AddPointLight();
    void AddSpotLight();
    void AddCube(CHEngine::ShaderHandle meshShader);
    void AddSphere(CHEngine::ShaderHandle impostorShader);
    void AddCameraEntity();
    void AddEmptyEntity();

    void AddUIOverlayCanvas();
    void AddUIWorldCanvas();
    void AddUIPanel(const CHEngine::UUID& canvas);
    void AddUIText(const CHEngine::UUID& canvas);
    void AddUIButton(const CHEngine::UUID& canvas);
    void AddUIImage(const CHEngine::UUID& canvas);
    void AddUISlider(const CHEngine::UUID& canvas);

    // On the selected entity.
    void SetSelectedPosition(float x, float y, float z);
    void SetSelectedRotation(float x, float y, float z);
    void SetSelectedScale(float x, float y, float z);
    void RenameSelected(const std::string& newName);

    // By name (AI commands) — affects all matching entities.
    void SelectByName(const std::string& name);
    void SetPositionByName(const std::string& name, float x, float y, float z);
    void SetRotationByName(const std::string& name, float x, float y, float z);
    void SetScaleByName(const std::string& name, float x, float y, float z);
    void SetColorByName(const std::string& name, float r, float g, float b, float a);
    void SetVisibleByName(const std::string& name, bool visible);
    void SetLightByName(const std::string& name, const std::string& lightType,
                        float r, float g, float b, float intensity);
    void RenameByName(const std::string& oldName, const std::string& newName);
    void DeleteByName(const std::string& name);

    // Texture slots on the selected submesh.
    void ApplyDiffuseTexture(size_t submesh_index, const std::string& filepath, CHEngine::ShaderHandle meshShader);
    void ClearDiffuseTexture(size_t submesh_index);
    void ApplySpecularTexture(size_t submesh_index, const std::string& filepath, CHEngine::ShaderHandle meshShader);
    void ClearSpecularTexture(size_t submesh_index);

    // Misc.
    void SetViewportFov(float fov_degrees);
    std::string GetSceneContextString() const;

private:
    bool m_IsActive = false;
};
