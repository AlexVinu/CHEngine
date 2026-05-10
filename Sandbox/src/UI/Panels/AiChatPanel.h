#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>

namespace Sandbox {

// ─────────────────────────────────────────────────────────────────────────────
// AiChatPanel
//
// Встроенный AI-помощник для генерации Lua-скриптов.
// Подключается к API совместимому с OpenAI (Moonshot / Kimi / Ollama Cloud).
//
// Использование:
//   AiChatPanel chat;
//   chat.Draw(currentEditorCode);
//   if (chat.HasPendingCode())
//       editor.SetText(chat.TakePendingCode());
// ─────────────────────────────────────────────────────────────────────────────
class AiChatPanel
{
public:
    // Вызывается когда AI сгенерировал код (для вставки в редактор)
    using InsertCodeCallback = std::function<void(const std::string& code)>;

    AiChatPanel();
    ~AiChatPanel();

    // Нарисовать панель. currentCode — текущий текст в редакторе (контекст для AI)
    void Draw(const std::string& currentCode);

    void SetInsertCallback(InsertCodeCallback cb) { m_InsertCallback = std::move(cb); }

    // API настройки
    void SetApiKey(const std::string& key)      { m_ApiKey = key; }
    void SetEndpoint(const std::string& url)    { m_Endpoint = url; }
    void SetModel(const std::string& model)     { m_Model = model; }

    const std::string& GetApiKey()   const { return m_ApiKey; }
    const std::string& GetEndpoint() const { return m_Endpoint; }
    const std::string& GetModel()    const { return m_Model; }

private:
    struct Message {
        bool  isUser = true;
        std::string text;
        bool  isCode = false;   // содержит Lua-код
    };

    void SendMessage();
    void RequestAsync(const std::string& userText, const std::string& editorCode);
    std::string CallApi(const std::string& body);
    std::string BuildRequestBody(const std::string& userText,
                                  const std::string& editorCode) const;
    static std::string ExtractContent(const std::string& jsonResponse);
    static std::string ExtractCodeBlock(const std::string& text);
    static std::string BuildSystemPrompt();

    void AppendMessage(bool isUser, const std::string& text, bool isCode = false);

    // UI state
    char m_InputBuf[1024]      = {};
    bool m_ScrollToBottom      = false;

    // Messages
    std::vector<Message> m_Messages;
    std::mutex           m_MessagesMutex;

    // API settings
    std::string m_ApiKey   = "";
    std::string m_Endpoint = "https://api.moonshot.cn/v1/chat/completions";
    std::string m_Model    = "moonshot-v1-8k";

    // Async
    std::atomic<bool> m_IsLoading{ false };
    std::thread       m_Worker;

    // Callback
    InsertCodeCallback m_InsertCallback;

    // Settings popup
    bool m_ShowSettings = false;
    char m_ApiKeyBuf[256]   = {};
    char m_EndpointBuf[512] = {};
    char m_ModelBuf[128]    = {};

    // Pending code from worker thread → main thread
    std::string m_PendingCode;
    bool        m_HasPending = false;
    std::mutex  m_PendingMutex;

    // Last editor code (set each Draw call)
    std::string m_LastEditorCode;
};

} // namespace Sandbox
