#pragma once

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <functional>

namespace Sandbox {

class SceneViewLayerHost;

// ─────────────────────────────────────────────────────────────────────────────
// GlobalAiOverlay
//
// Floating AI assistant overlay triggered by double-tap Z.
// Understands natural language intent and can rearrange the editor layout.
//
// Layout presets the AI can apply:
//   "script_focus" — Viewport 25% | ScriptEditor 50% | ContentBrowser 25%
//   "model_focus"  — Large viewport, inspector/properties on right
//   "default"      — Standard layout
//
// AI response format (JSON):
//   {"layout":"script_focus","entity":"Cube","message":"Открываю скрипт-режим"}
// ─────────────────────────────────────────────────────────────────────────────
class GlobalAiOverlay
{
public:
    GlobalAiOverlay();
    ~GlobalAiOverlay();

    void Toggle();
    bool IsOpen() const { return m_IsOpen; }

    // Call every ImGui frame (after tiling panels are drawn)
    void Draw(SceneViewLayerHost& host);

    // Settings — set before first use
    void SetApiKey(const std::string& key)      { m_ApiKey   = key; }
    void SetEndpoint(const std::string& ep)      { m_Endpoint = ep;  }
    void SetModel(const std::string& model)      { m_Model    = model; }

    const std::string& GetApiKey()   const { return m_ApiKey; }
    const std::string& GetEndpoint() const { return m_Endpoint; }
    const std::string& GetModel()    const { return m_Model; }

private:
    struct Message {
        bool        isUser = false;
        std::string text;
    };

    bool m_IsOpen     = false;
    bool m_FocusInput = false;
    char m_InputBuf[512] = {};

    std::vector<Message> m_Messages;
    std::string          m_Status;   // "" | "thinking" | "error"

    // API settings
    std::string m_ApiKey   = "b5e9ffdfdfd14c26bce58d680d33253e.2BLBP_ZahuBqLflPqnhqQ7v3";
    std::string m_Endpoint = "https://ollama.com/v1/chat/completions";
    std::string m_Model    = "gemma3:12b";

    // Settings panel state
    bool m_ShowSettings = false;
    char m_EndpointBuf[256] = {};
    char m_ModelBuf[128]    = {};
    char m_ApiKeyBuf[256]   = {};

    // Thread-safe response from worker thread
    std::mutex  m_Mutex;
    std::string m_PendingResponse;
    bool        m_HasPending = false;
    std::thread m_Worker;

    // ── Internal ─────────────────────────────────────────────────────────────
    void Submit(const std::string& userMsg, SceneViewLayerHost& host);
    void WorkerFn(std::string userMsg);
    void ApplyResponse(const std::string& raw, SceneViewLayerHost& host);

    std::string BuildRequestBody(const std::string& userMsg) const;
    std::string CallHttp(const std::string& body);
    std::string ExtractContent(const std::string& httpResponse);

    // Minimal JSON field extractor
    static std::string JsonField(const std::string& json, const std::string& key);
    static std::string JsonEscape(const std::string& s);

    void DrawSettingsPanel();
};

} // namespace Sandbox
