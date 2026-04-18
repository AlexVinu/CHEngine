#pragma once

#include <string>

class SceneViewLayer;

namespace SceneViewLayerIO {

void SaveScene(SceneViewLayer& layer);
void LoadScene(SceneViewLayer& layer, const std::string& path = "");
void AutoSaveForRestart(SceneViewLayer& layer);
void TryRestoreSession(SceneViewLayer& layer);
void ImportModel(SceneViewLayer& layer, const std::string& filepath);
void LoadRecentFilesList();

} // namespace SceneViewLayerIO
