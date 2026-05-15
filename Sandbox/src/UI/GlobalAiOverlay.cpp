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
        "• \"Добавь куб\"\n"
        "• \"Буду работать над скриптами объекта Cube\"\n"
        "• \"Хочу работать с текстурами\"\n"
        "• \"Верни стандартный layout\"";
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
static const char* kSystemPrompt = R"(You are an assistant inside CHEngine 3D editor. Parse user intent and return ONLY a JSON object — no markdown, no explanation, nothing else.

JSON format:
{"layout":"<preset>","creates":"<comma_separated_list_or_empty>","entity":"<name_or_empty>","focus_on":"<entity_name_to_focus_camera_or_empty>","message":"<max_10_words_same_language>"}

━━━ LAYOUT PRESETS ━━━
"script_focus"  → scripts/code/lua / скрипт/код
"uv_focus"      → textures/UV/materials / текстуры/UV/материалы
"model_focus"   → scene/objects/3D/lights / сцена/объекты/свет
"default"       → reset/standard / сброс/стандарт

━━━ CREATES FIELD ━━━
Comma-separated list of objects to create. Supported values:
cube, sphere, empty, camera, dir_light, point_light, spot_light
Examples: "cube", "sphere,camera", "cube,sphere,point_light"
If nothing to create → ""

Mapping:
- cube/куб/box → cube
- sphere/сфера/шар/мяч → sphere
- empty/пустой → empty
- camera/камера → camera
- directional/направленный → dir_light
- point light/точечный → point_light
- spot/прожектор → spot_light

━━━ FOCUS_ON FIELD ━━━
If user says "camera looks at X", "наведи камеру на X", "камера смотрит на X" — set focus_on to that object name.
Otherwise "".

━━━ ENTITY FIELD ━━━
Exact name of existing object to select (for script/UV work). "" if not applicable.

━━━ EXAMPLES ━━━
User: "добавь шарик, камеру и куб, пусть камера смотрит на шарик"
{"layout":"model_focus","creates":"sphere,camera,cube","entity":"","focus_on":"Sphere","message":"Добавил шар, камеру, куб; камера → шар"}

User: "добавь куб"
{"layout":"model_focus","creates":"cube","entity":"","focus_on":"","message":"Добавил куб"}

User: "создай сферу и точечный свет"
{"layout":"model_focus","creates":"sphere,point_light","entity":"","focus_on":"","message":"Добавил сферу и свет"}

User: "буду писать скрипт для Sphere"
{"layout":"script_focus","creates":"","entity":"Sphere","focus_on":"","message":"Скрипт-режим для Sphere"}

User: "хочу работать с текстурами Cube"
{"layout":"uv_focus","creates":"","entity":"Cube","focus_on":"","message":"UV-режим для Cube"}

User: "верни стандартный layout"
{"layout":"default","creates":"","entity":"","focus_on":"","message":"Стандартный layout восстановлен"}
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

    std::string layout   = JsonField(json, "layout");
    std::string creates  = JsonField(json, "creates");
    std::string entity   = JsonField(json, "entity");
    std::string focus_on = JsonField(json, "focus_on");
    std::string message  = JsonField(json, "message");

    if (layout.empty()) layout = "default";
    if (message.empty()) message = "Готово";

    // Create objects — parse comma-separated list
    if (!creates.empty())
    {
        auto createOne = [&](const std::string& type)
        {
            if      (type == "cube")        host.AddCubePrimitive();
            else if (type == "sphere")      host.AddSpherePrimitive();
            else if (type == "empty")       host.AddEmptyEntity();
            else if (type == "camera")      host.AddCameraEntity();
            else if (type == "dir_light")   host.AddDirectionalLight();
            else if (type == "point_light") host.AddPointLight();
            else if (type == "spot_light")  host.AddSpotLight();
        };

        // Split by comma
        std::string token;
        for (char c : creates)
        {
            if (c == ',') { if (!token.empty()) { createOne(token); token.clear(); } }
            else if (c != ' ') token += c;
        }
        if (!token.empty()) createOne(token);
    }

    // Apply layout
    host.ApplyLayoutPreset(layout);

    // Select entity by name if provided
    if (!entity.empty())
        host.SelectEntityByName(entity);

    // If script_focus, also open the entity's script in editor
    if (layout == "script_focus" && !entity.empty())
        host.OpenScriptForEntity(entity);

    // Focus viewport camera on target entity
    if (!focus_on.empty())
    {
        host.SelectEntityByName(focus_on);
        host.FocusOnSelected();
    }

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

    // macOS focus recovery: если окно кликнули после потери фокуса (запись экрана и т.п.)
    // — возвращаем фокус на поле ввода
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        !ImGui::IsAnyItemActive() &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        m_FocusInput = true;
    }

    // ── Header bar ────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.75f, 1.00f, 1.0f));
    ImGui::SetWindowFontScale(1.15f);
    ImGui::Text("  AI Assistant");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::SameLine(overlayW - 60.0f);
    if (ImGui::SmallButton(m_ShowSettings ? "✕ cfg" : "⚙ cfg"))
        m_ShowSettings = !m_ShowSettings;

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
