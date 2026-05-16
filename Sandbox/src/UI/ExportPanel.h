#pragma once

#include "ExportManager.h"
#include "SceneViewLayerHost.h"

#include <imgui.h>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>

namespace Sandbox {

// ─────────────────────────────────────────────────────────────────────────────
// ExportPanel — floating dialog for exporting the game
//
// Open via: m_ExportPanel.Open()
// Draw via: m_ExportPanel.Draw(host)  — call every ImGui frame
// ─────────────────────────────────────────────────────────────────────────────
class ExportPanel {
public:
    void Open();
    void Draw(SceneViewLayerHost& host);
    bool IsOpen() const { return m_Open; }

private:
    bool m_Open = false;

    // Settings
    char m_OutputDir[512]    = {};
    char m_GameTitle[128]    = "My Game";
    int  m_Width             = 1280;
    int  m_Height            = 720;

    // Export progress (runs on worker thread)
    std::thread          m_Worker;
    std::mutex           m_Mutex;
    float                m_Progress   = 0.0f;
    std::string          m_StatusMsg;
    bool                 m_Exporting  = false;
    bool                 m_Done       = false;
    bool                 m_Success    = false;

    void StartExport(SceneViewLayerHost& host);
};

} // namespace Sandbox
