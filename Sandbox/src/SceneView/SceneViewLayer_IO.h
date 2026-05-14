#pragma once

#include <string>

class SceneViewLayer;

namespace SceneViewLayerIO {

void SaveScene(SceneViewLayer& layer);
void LoadScene(SceneViewLayer& layer, const std::string& path = "");
/// Load scene without touching recent files or camera overlay.
void LoadSceneSilent(SceneViewLayer& layer, const std::string& absPath);
void AutoSaveForRestart(SceneViewLayer& layer);
void TryRestoreSession(SceneViewLayer& layer);
void ImportModel(SceneViewLayer& layer, const std::string& filepath);

} // namespace SceneViewLayerIO
