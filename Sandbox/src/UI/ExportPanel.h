#pragma once

#include "ExportManager.h"

#include <imgui.h>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>

namespace Sandbox {

struct EditorContext;

// ─────────────────────────────────────────────────────────────────────────────
// ExportPanel — floating dialog for exporting the game
//
// Open via: ctx.Export.Open()
// Draw via: ctx.Export.Draw(ctx)  — call every ImGui frame
// ─────────────────────────────────────────────────────────────────────────────
class ExportPanel {
public:
    void Open();
    void Draw(EditorContext& ctx);
    bool IsOpen() const { return m_Open; }

private:
    bool m_Open = false;

    // Settings
    char m_OutputDir[512]    = {};
    char m_GameTitle[128]    = "My Game";
    char m_BundleId[256]     = "com.chengine.game";
    int  m_Width             = 1280;
    int  m_Height            = 720;

    // Startup scene picker (relative pak paths, e.g. "Scenes/main.chscene").
    std::vector<std::string> m_Scenes;
    int                      m_SceneSel = -1;
    bool                     m_ScenesScanned = false;

    void RefreshScenes();

    // Export progress (runs on worker thread)
    std::thread          m_Worker;
    std::mutex           m_Mutex;
    float                m_Progress   = 0.0f;
    std::string          m_StatusMsg;
    bool                 m_Exporting  = false;
    bool                 m_Done       = false;
    bool                 m_Success    = false;

    void StartExport(EditorContext& ctx);
};

} // namespace Sandbox
