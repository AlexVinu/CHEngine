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
    bool  IsOpen()        const { return m_IsOpen; }
    bool  IsVisible()     const { return m_AnimProgress > 0.0f; }  // true during animation too
    float GetAnimProgress() const { return m_AnimProgress; }

    // Call every ImGui frame (after tiling panels are drawn)
    void Draw(SceneViewLayerHost& host);

    // Settings — set before first use
    void SetApiKey(const std::string& key)      { m_ApiKey   = key; }
    void SetEndpoint(const std::string& ep)      { m_Endpoint = ep;  }
    void SetModel(const std::string& model)      { m_Model    = model; }

    const std::string& GetApiKey()   const { return m_ApiKey; }
    const std::string& GetEndpoint() const { return m_Endpoint; }
    const std::string& GetModel()    const { return m_Model; }

    // Public so WorkerFn (which is a member) can call it on the worker thread.
    static std::string JsonEscape(const std::string& s);

private:
    struct Message {
        bool        isUser = false;
        std::string text;
    };

    bool  m_IsOpen       = false;
    bool  m_FocusInput   = false;
    float m_AnimProgress = 0.0f;  // 0=hidden, 1=fully visible (drives position + alpha + dim)
    char  m_InputBuf[512] = {};

    std::vector<Message> m_Messages;
    std::string          m_Status;   // "" | "thinking" | "error"

    // API settings — loaded from env vars CHE_AI_API_KEY, CHE_AI_ENDPOINT, CHE_AI_MODEL
    // Fallback defaults used when env vars are not set
    std::string m_ApiKey   = "";
    std::string m_Endpoint = "https://ollama.com/v1/chat/completions";
    std::string m_Model    = "gemma4:31b";

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

    // API conversation history (role + content pairs, last N turns)
    struct HistoryEntry { std::string role; std::string content; };
    std::vector<HistoryEntry> m_ApiHistory;
    static constexpr int kMaxHistoryTurns = 4;  // keep last 4 exchanges (8 messages)

    // Scene context snapshot passed to worker thread
    std::string m_SceneContextSnapshot;

    // ── Internal ─────────────────────────────────────────────────────────────
    void Submit(const std::string& userMsg, SceneViewLayerHost& host);
    // All API fields passed by value — thread has no shared mutable state to race on.
    struct WorkerArgs {
        std::string userMsg;
        std::string apiKey;
        std::string endpoint;
        std::string model;
        std::string sceneContext;
        std::vector<HistoryEntry> history;
    };
    void WorkerFn(WorkerArgs args);
    void ApplyResponse(const std::string& raw, SceneViewLayerHost& host);

    std::string BuildRequestBody(const std::string& userMsg) const;
    std::string CallHttp(const std::string& body);
    std::string ExtractContent(const std::string& httpResponse);

    // Minimal JSON field extractor
    static std::string JsonField(const std::string& json, const std::string& key);

    void DrawSettingsPanel();
};

} // namespace Sandbox
