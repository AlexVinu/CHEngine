#include "GlobalAiOverlay.h"
#include "SceneViewLayerHost.h"

#include <imgui.h>
#include <imgui_internal.h>

#ifdef CHE_HAS_CURL
#include <curl/curl.h>
#endif

#include <sstream>
#include <cstring>
#include <algorithm>

namespace Sandbox {

// ─────────────────────────────────────────────────────────────────────────────
// CURL write callback
// ─────────────────────────────────────────────────────────────────────────────
#ifdef CHE_HAS_CURL
static size_t CurlWriteCb(char* ptr, size_t size, size_t nmemb, std::string* out)
{
    out->append(ptr, size * nmemb);
    return size * nmemb;
}
#endif

// ─────────────────────────────────────────────────────────────────────────────
std::string GlobalAiOverlay::JsonEscape(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 16);
    for (unsigned char c : s) {
        switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:
            if (c < 0x20) { char buf[8]; snprintf(buf, sizeof(buf), "\\u%04x", c); out += buf; }
            else out += (char)c;
            break;
        }
    }
    return out;
}

// Extracts value of "key" from flat JSON string (handles escaped quotes)
std::string GlobalAiOverlay::JsonField(const std::string& json, const std::string& key)
{
    std::string searchKey = "\"" + key + "\"";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return "";
    pos += searchKey.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == ':')) ++pos;
    if (pos >= json.size() || json[pos] != '"') return "";
    ++pos;
    std::string result;
    while (pos < json.size() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.size()) { ++pos; result += json[pos]; }
        else result += json[pos];
        ++pos;
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
GlobalAiOverlay::GlobalAiOverlay()
{
    Message welcome;
    welcome.isUser = false;
    welcome.text   =
        "Привет! Я помогу настроить рабочее пространство.\n\n"
        "Примеры:\n"
        "• \"Буду работать над скриптами объекта Cube\"\n"
        "• \"Хочу расставить объекты на сцене\"\n"
        "• \"Верни стандартный layout\"\n\n"
        "Введи API ключ в настройках (кнопка ⚙ справа).";
    m_Messages.push_back(welcome);

    std::strncpy(m_EndpointBuf, m_Endpoint.c_str(), sizeof(m_EndpointBuf) - 1);
    std::strncpy(m_ModelBuf,    m_Model.c_str(),    sizeof(m_ModelBuf)    - 1);
}

GlobalAiOverlay::~GlobalAiOverlay()
{
    if (m_Worker.joinable())
        m_Worker.detach();
}

void GlobalAiOverlay::Toggle()
{
    m_IsOpen = !m_IsOpen;
    if (m_IsOpen)
        m_FocusInput = true;
}

// ─────────────────────────────────────────────────────────────────────────────
// System prompt
// ─────────────────────────────────────────────────────────────────────────────
static const char* kSystemPrompt = R"(You are a layout assistant inside CHEngine 3D editor. Classify the user intent and return ONLY a JSON object, no other text.

LAYOUT PRESETS — choose exactly one:

"script_focus"
  USE FOR: scripting, code, lua, programming, logic, скрипт, код, программирование, логика
  PANELS: small Viewport left, large ScriptEditor center, ContentBrowser right

"uv_focus"
  USE FOR: textures, UV, unwrap, materials, mapping, текстуры, UV, развёртка, материалы, текстурирование
  PANELS: Viewport left, large UVEditor right, Properties below

"model_focus"
  USE FOR: modeling, scene, placing objects, transform, 3D, layout, расстановка, сцена, моделирование, объекты, трансформация
  PANELS: large Viewport, ContentBrowser below, Inspector right

"default"
  USE FOR: reset, standard, back to normal, стандарт, сброс, обычный

DECISION RULES (strict):
- Any mention of texture/UV/material/unwrap → ALWAYS "uv_focus", never anything else
- Any mention of script/code/lua/logic → ALWAYS "script_focus"
- Any mention of placing/moving/scene/3D objects → "model_focus"
- Reset request → "default"
- Ambiguous → "default"

JSON format (respond with ONLY this, no markdown, no explanation):
{"layout":"<preset>","entity":"<exact_entity_name_or_empty_string>","message":"<max_8_words_same_language_as_user>"}

EXAMPLES:
User: "хочу работать с текстурами объекта Cube"
{"layout":"uv_focus","entity":"Cube","message":"Открываю UV-режим для Cube"}

User: "буду работать над скриптами объекта Sphere"
{"layout":"script_focus","entity":"Sphere","message":"Открываю скрипт-режим для Sphere"}

User: "хочу расставить объекты на сцене"
{"layout":"model_focus","entity":"","message":"Переключаю на режим сцены"}

User: "I want to work on textures"
{"layout":"uv_focus","entity":"","message":"Switching to UV editor mode"}

User: "верни стандартный layout"
{"layout":"default","entity":"","message":"Восстановил стандартный layout"}

User: "материалы и UV развёртка для Wall"
{"layout":"uv_focus","entity":"Wall","message":"UV-режим для Wall"}
)";

// ─────────────────────────────────────────────────────────────────────────────
std::string GlobalAiOverlay::BuildRequestBody(const std::string& userMsg) const
{
    std::ostringstream ss;
    ss << "{"
       << "\"model\":\"" << JsonEscape(m_Model) << "\","
       << "\"temperature\":0.1,"
       << "\"max_tokens\":256,"
       << "\"messages\":["
       <<   "{\"role\":\"system\",\"content\":\"" << JsonEscape(kSystemPrompt) << "\"},"
       <<   "{\"role\":\"user\",\"content\":\"" << JsonEscape(userMsg) << "\"}"
       << "]}";
    return ss.str();
}

// ─────────────────────────────────────────────────────────────────────────────
std::string GlobalAiOverlay::CallHttp(const std::string& body)
{
#ifdef CHE_HAS_CURL
    CURL* curl = curl_easy_init();
    if (!curl) return "__ERR__curl init failed";

    std::string response;
    std::string authHeader  = "Authorization: Bearer " + m_ApiKey;
    std::string contentType = "Content-Type: application/json";

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, authHeader.c_str());
    headers = curl_slist_append(headers, contentType.c_str());

    curl_easy_setopt(curl, CURLOPT_URL,            m_Endpoint.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,     headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,     body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,  (long)body.size());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  CurlWriteCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        30L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
        return "__ERR__" + std::string(curl_easy_strerror(res));

    return "__HTTP__" + std::to_string(httpCode) + "__BODY__" + response;
#else
    (void)body;
    return "__ERR__CURL not available in this build";
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
std::string GlobalAiOverlay::ExtractContent(const std::string& httpResponse)
{
    if (httpResponse.rfind("__ERR__", 0) == 0)
        return "__ERR__" + httpResponse.substr(7);

    size_t bodyPos = httpResponse.find("__BODY__");
    if (bodyPos == std::string::npos) return "__ERR__no body";
    const std::string& json = httpResponse.substr(bodyPos + 8);

    // OpenAI-compatible: choices[0].message.content
    auto extractStr = [&](const std::string& key) -> std::string {
        std::string searchKey = "\"" + key + "\":\"";
        size_t pos = json.find(searchKey);
        if (pos == std::string::npos) return "";
        pos += searchKey.size();
        std::string result;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\' && pos + 1 < json.size()) {
                ++pos;
                switch (json[pos]) {
                case 'n': result += '\n'; break;
                case 't': result += '\t'; break;
                case 'u': {
                    if (pos + 4 < json.size()) {
                        int cp = 0;
                        sscanf(json.c_str() + pos + 1, "%4x", &cp);
                        if (cp < 128) result += (char)cp;
                        pos += 4;
                    }
                    break;
                }
                default: result += json[pos]; break;
                }
            } else {
                result += json[pos];
            }
            ++pos;
        }
        return result;
    };

    std::string content = extractStr("content");
    if (content.empty()) content = extractStr("message");
    if (content.empty()) return "__ERR__no content in response: " + json.substr(0, 200);
    return content;
}

// ─────────────────────────────────────────────────────────────────────────────
void GlobalAiOverlay::WorkerFn(std::string userMsg)
{
    std::string body    = BuildRequestBody(userMsg);
    std::string httpRaw = CallHttp(body);
    std::string content = ExtractContent(httpRaw);

    std::lock_guard<std::mutex> lock(m_Mutex);
    m_PendingResponse = content;
    m_HasPending      = true;
}

// ─────────────────────────────────────────────────────────────────────────────
void GlobalAiOverlay::Submit(const std::string& userMsg, SceneViewLayerHost& /*host*/)
{
    if (m_Worker.joinable())
        m_Worker.join();

    m_Status = "thinking";
    m_Worker = std::thread(&GlobalAiOverlay::WorkerFn, this, userMsg);
}

// ─────────────────────────────────────────────────────────────────────────────
void GlobalAiOverlay::ApplyResponse(const std::string& raw, SceneViewLayerHost& host)
{
    if (raw.rfind("__ERR__", 0) == 0)
    {
        m_Messages.push_back({ false, "Ошибка: " + raw.substr(7) });
        m_Status = "error";
        return;
    }

    // Find JSON object in response (AI might add extra text)
    std::string json = raw;
    size_t jStart = json.find('{');
    size_t jEnd   = json.rfind('}');
    if (jStart != std::string::npos && jEnd != std::string::npos && jEnd > jStart)
        json = json.substr(jStart, jEnd - jStart + 1);

    std::string layout  = JsonField(json, "layout");
    std::string entity  = JsonField(json, "entity");
    std::string message = JsonField(json, "message");

    if (layout.empty()) layout = "default";
    if (message.empty()) message = "Layout применён: " + layout;

    // Apply layout
    host.ApplyLayoutPreset(layout);

    // Select entity by name if provided
    if (!entity.empty())
        host.SelectEntityByName(entity);

    // If script_focus, also open the entity's script in editor
    if (layout == "script_focus" && !entity.empty())
        host.OpenScriptForEntity(entity);

    m_Messages.push_back({ false, message });
    m_Status.clear();
}

// ─────────────────────────────────────────────────────────────────────────────
void GlobalAiOverlay::DrawSettingsPanel()
{
    ImGui::Separator();
    ImGui::TextDisabled("API Settings");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("Endpoint##gai_ep", m_EndpointBuf, sizeof(m_EndpointBuf)))
        m_Endpoint = m_EndpointBuf;
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("Model##gai_mdl", m_ModelBuf, sizeof(m_ModelBuf)))
        m_Model = m_ModelBuf;
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("API Key##gai_key", m_ApiKeyBuf, sizeof(m_ApiKeyBuf),
                         ImGuiInputTextFlags_Password))
        m_ApiKey = m_ApiKeyBuf;
    ImGui::Separator();
}

// ─────────────────────────────────────────────────────────────────────────────
void GlobalAiOverlay::Draw(SceneViewLayerHost& host)
{
    if (!m_IsOpen) return;

    // Poll pending response from worker thread
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (m_HasPending)
        {
            ApplyResponse(m_PendingResponse, host);
            m_PendingResponse.clear();
            m_HasPending = false;
        }
    }

    // Position: centered, floating above the bottom bar
    ImGuiIO& io = ImGui::GetIO();
    const float W = io.DisplaySize.x;
    const float H = io.DisplaySize.y;
    const float overlayW = std::min(W * 0.60f, 800.0f);
    const float overlayH = m_ShowSettings ? 340.0f : 260.0f;
    const float overlayX = (W - overlayW) * 0.5f;
    const float overlayY = H - overlayH - 48.0f;

    ImGui::SetNextWindowPos(ImVec2(overlayX, overlayY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(overlayW, overlayH), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.95f);

    const ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoTitleBar  | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove      | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg,    ImVec4(0.10f, 0.10f, 0.12f, 0.97f));
    ImGui::PushStyleColor(ImGuiCol_Border,      ImVec4(0.30f, 0.45f, 0.80f, 0.70f));

    bool open = true;
    if (!ImGui::Begin("##global_ai_overlay", &open, kFlags))
    {
        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        return;
    }

    // Always render above every other panel
    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

    // ── Header bar ────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.75f, 1.00f, 1.0f));
    ImGui::TextUnformatted("  AI Assistant  [double-Z to close]");
    ImGui::PopStyleColor();

    ImGui::SameLine(overlayW - 60.0f);
    if (ImGui::SmallButton(m_ShowSettings ? "✕ cfg" : "⚙ cfg"))
        m_ShowSettings = !m_ShowSettings;

    ImGui::SameLine();
    if (ImGui::SmallButton("✕"))
        m_IsOpen = false;

    if (m_ShowSettings)
        DrawSettingsPanel();

    ImGui::Separator();

    // ── Chat history ──────────────────────────────────────────────────────────
    const float inputH = ImGui::GetFrameHeightWithSpacing() + 6.0f;
    ImGui::BeginChild("##gai_chat", ImVec2(-1.0f, -inputH), false,
                      ImGuiWindowFlags_NoScrollbar);

    for (auto& msg : m_Messages)
    {
        if (msg.isUser)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.80f, 0.90f, 1.00f, 1.0f));
            ImGui::TextWrapped(">  %s", msg.text.c_str());
            ImGui::PopStyleColor();
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.90f, 0.65f, 1.0f));
            ImGui::TextWrapped("%s", msg.text.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::Spacing();
    }

    if (m_Status == "thinking")
    {
        float t = (float)ImGui::GetTime();
        int dots = (int)(t * 2.0f) % 4;
        std::string anim = "Думаю" + std::string(dots, '.');
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.60f, 1.0f));
        ImGui::TextUnformatted(anim.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();

    // ── Input field ───────────────────────────────────────────────────────────
    ImGui::Separator();

    if (m_FocusInput)
    {
        ImGui::SetKeyboardFocusHere();
        m_FocusInput = false;
    }

    bool send = false;
    ImGui::SetNextItemWidth(-70.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
    if (ImGui::InputText("##gai_input", m_InputBuf, sizeof(m_InputBuf),
                         ImGuiInputTextFlags_EnterReturnsTrue))
        send = true;
    ImGui::PopStyleColor();

    ImGui::SameLine();
    const bool busy = (m_Status == "thinking");
    if (busy) ImGui::BeginDisabled();
    if (ImGui::Button("Send##gai", ImVec2(60.0f, 0.0f)) || send)
    {
        if (m_InputBuf[0] != '\0' && !busy)
        {
            std::string userMsg = m_InputBuf;
            m_Messages.push_back({ true, userMsg });
            m_InputBuf[0] = '\0';
            m_FocusInput = true;
            Submit(userMsg, host);
        }
    }
    if (busy) ImGui::EndDisabled();

    // Close on Escape (only when input is not focused by something else)
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) &&
        !ImGui::IsAnyItemActive())
        m_IsOpen = false;

    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);

    if (!open) m_IsOpen = false;
}

} // namespace Sandbox
