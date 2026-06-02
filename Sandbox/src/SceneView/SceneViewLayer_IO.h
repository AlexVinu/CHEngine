#pragma once

#include <CHEngine.h>

#include <string>
#include <vector>

struct EditorWorldContext;
namespace Sandbox { struct EditorContext; }

namespace SceneViewLayerIO {

// ── Active-scene I/O ──────────────────────────────────────────────────────────
void SaveScene(Sandbox::EditorContext& ctx);
void LoadScene(Sandbox::EditorContext& ctx, const std::string& path = "");
/// Load scene without touching recent files or camera overlay.
void LoadSceneSilent(Sandbox::EditorContext& ctx, const std::string& absPath);
void AutoSaveForRestart(Sandbox::EditorContext& ctx);
void TryRestoreSession(Sandbox::EditorContext& ctx);
void ImportModel(Sandbox::EditorContext& ctx, const std::string& filepath);
void OpenImportModelDialog(Sandbox::EditorContext& ctx);

// ── Session (tab) management ──────────────────────────────────────────────────
std::vector<EditorWorldContext*> GetSceneSessions(Sandbox::EditorContext& ctx);
void AddSceneSession(Sandbox::EditorContext& ctx);
void CloseSceneSession(Sandbox::EditorContext& ctx, size_t session_index);

// ── Scene file management (Scene Browser) ─────────────────────────────────────
/// Open a scene file in a new tab (or switch to existing tab bound to that file).
void OpenSceneFile(Sandbox::EditorContext& ctx, const std::string& relOrAbsPath);
void NewSceneFile(Sandbox::EditorContext& ctx);
void DeleteSceneFile(Sandbox::EditorContext& ctx, const std::string& rel);
void RenameSceneFile(Sandbox::EditorContext& ctx, const std::string& oldRel, const std::string& newName);
void SetStartupSceneFile(Sandbox::EditorContext& ctx, const std::string& rel);

// ── Renderer API switch (saves prefs + restarts engine) ───────────────────────
void SelectRendererApi(Sandbox::EditorContext& ctx, CHEngine::ERenderAPI api);

} // namespace SceneViewLayerIO
