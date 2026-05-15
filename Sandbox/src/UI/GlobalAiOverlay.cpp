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
static const char* kSystemPrompt = R"(You are a command parser for CHEngine 3D editor. Parse the user request and return ONLY a valid JSON object. No markdown, no explanation, no extra text — ONLY the JSON.

═══════════════════════════════════════════
JSON SCHEMA (all fields required):
{
  "layout":     "<preset>",
  "creates":    "<comma_separated_or_empty>",
  "script_for": "<entity_name_or_empty>",
  "focus_on":   "<entity_name_or_empty>",
  "entity":     "<entity_name_or_empty>",
  "message":    "<short_confirmation_same_language_max_12_words>"
}
═══════════════════════════════════════════

━━━ FIELD: layout ━━━
Choose EXACTLY ONE. Default to "model_focus" when in doubt about objects/scene.

  "model_focus"  — Use when: creating objects, working with 3D scene, lights, camera, transforms
  "script_focus" — Use ONLY when: user explicitly says script/code/lua/logic
  "uv_focus"     — Use ONLY when: user explicitly says texture/UV/material/unwrap
  "default"      — Use ONLY when: user says reset/standard layout

⚠ ANTI-HALLUCINATION RULES FOR LAYOUT:
  - "добавь куб" → "model_focus"  (NOT uv_focus, NOT script_focus)
  - "создай сцену" → "model_focus"
  - "добавь свет" → "model_focus"
  - NEVER use "uv_focus" unless words: texture, UV, unwrap, material, текстур, материал, UV, развёртк
  - NEVER use "script_focus" unless words: script, код, lua, скрипт, logic, логик, code

━━━ FIELD: creates ━━━
Comma-separated list of objects to create. Leave "" if user is NOT asking to add new objects.
Values: cube, sphere, empty, camera, dir_light, point_light, spot_light

Word → value mapping:
  cube/куб/кубик/box → cube
  sphere/шар/шарик/мяч/сфера → sphere
  empty/пустой/empty object → empty
  camera/камера → camera
  directional light/направленный свет → dir_light
  point light/точечный свет → point_light
  spot light/прожектор/spot → spot_light

Order matters: create objects BEFORE focusing camera on them.
Example: "sphere,cube,camera" creates sphere first, then cube, then camera.

━━━ FIELD: script_for ━━━
Entity name that needs a script created and attached.
Use when user says "добавь скрипт [объекту]", "пусть [объект] двигается/прыгает/вращается".
The entity will be created from "creates" field first, then script attached.
Default names after creation: cube→"Cube", sphere→"Sphere", camera→"Camera"
Leave "" if no script needed.

━━━ FIELD: focus_on ━━━
Entity name to focus the viewport camera on.
Use when: "камера смотрит на X", "наведи камеру на X", "camera looks at X", "camera targets X"
Leave "" if not requested.

━━━ FIELD: entity ━━━
Name of EXISTING entity to select (for script editing or UV work).
Leave "" when creating new objects.

═══════════════════════════════════════════
EXAMPLES:
═══════════════════════════════════════════

User: "добавь куб"
{"layout":"model_focus","creates":"cube","script_for":"","focus_on":"","entity":"","message":"Добавил куб"}

User: "создай сферу и точечный свет"
{"layout":"model_focus","creates":"sphere,point_light","script_for":"","focus_on":"","entity":"","message":"Добавил сферу и точечный свет"}

User: "создай сцену: шарик и кубик, камера смотрит на шарик, кубик двигается — добавь ему скрипт"
{"layout":"script_focus","creates":"sphere,cube,camera","script_for":"Cube","focus_on":"Sphere","entity":"","message":"Создал сцену: шар+куб+камера, скрипт на кубе"}

User: "добавь камеру направленный свет и куб"
{"layout":"model_focus","creates":"cube,dir_light,camera","script_for":"","focus_on":"","entity":"","message":"Добавил куб, свет и камеру"}

User: "хочу написать скрипт для объекта Cube"
{"layout":"script_focus","creates":"","script_for":"","focus_on":"","entity":"Cube","message":"Открываю скрипт для Cube"}

User: "хочу работать с текстурами Sphere"
{"layout":"uv_focus","creates":"","script_for":"","focus_on":"","entity":"Sphere","message":"UV-режим для Sphere"}

User: "верни стандартный layout"
{"layout":"default","creates":"","script_for":"","focus_on":"","entity":"","message":"Восстановил стандартный layout"}

User: "добавь пустой объект и камеру, камера смотрит на объект"
{"layout":"model_focus","creates":"empty,camera","script_for":"","focus_on":"New Object","entity":"","message":"Добавил объект и камеру"}
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

    std::string layout     = JsonField(json, "layout");
    std::string creates    = JsonField(json, "creates");
    std::string script_for = JsonField(json, "script_for");
    std::string entity     = JsonField(json, "entity");
    std::string focus_on   = JsonField(json, "focus_on");
    std::string message    = JsonField(json, "message");

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

    // Create and attach script to a newly created (or existing) entity
    if (!script_for.empty())
        host.CreateAndAttachScriptToEntityByName(script_for);

    // If script_focus, open the entity's existing script in editor
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
// Easing functions
// ─────────────────────────────────────────────────────────────────────────────
static float EaseOutCubic(float t)
{
    float f = 1.0f - t;
    return 1.0f - f * f * f;
}
static float EaseInCubic(float t) { return t * t * t; }

// ─────────────────────────────────────────────────────────────────────────────
void GlobalAiOverlay::Draw(SceneViewLayerHost& host)
{
    ImGuiIO& io = ImGui::GetIO();
    const float dt = io.DeltaTime;

    // ── Animate progress ──────────────────────────────────────────────────────
    const float kAnimSpeed = 1.0f / 0.22f;  // 220ms open/close
    if (m_IsOpen)
        m_AnimProgress = std::min(1.0f, m_AnimProgress + dt * kAnimSpeed);
    else
        m_AnimProgress = std::max(0.0f, m_AnimProgress - dt * kAnimSpeed);

    // Nothing to draw when fully closed
    if (m_AnimProgress <= 0.0f) return;

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

    const float W = io.DisplaySize.x;
    const float H = io.DisplaySize.y;

    // Eased progress for position (ease-out when opening, ease-in when closing)
    float easedPos = m_IsOpen ? EaseOutCubic(m_AnimProgress) : EaseInCubic(m_AnimProgress);
    float easedAlpha = m_AnimProgress; // linear alpha is fine

    // ── Background dim ────────────────────────────────────────────────────────
    // Fullscreen transparent window drawn BEFORE the overlay (overlay goes on top)
    {
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(W, H), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("##ai_dim_bg", nullptr,
            ImGuiWindowFlags_NoTitleBar    | ImGuiWindowFlags_NoResize      |
            ImGuiWindowFlags_NoMove        | ImGuiWindowFlags_NoScrollbar   |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav       |
            ImGuiWindowFlags_NoInputs      | ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoDecoration  | ImGuiWindowFlags_NoFocusOnAppearing);
        ImGui::PopStyleVar();
        ImU32 dimColor = IM_COL32(0, 0, 0, (int)(160 * easedAlpha));
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(0, 0), ImVec2(W, H), dimColor);
        ImGui::End();
    }

    // ── Overlay window ────────────────────────────────────────────────────────
    const float overlayW = std::min(W * 0.60f, 800.0f);
    const float overlayH = m_ShowSettings ? 340.0f : 260.0f;
    const float overlayX = (W - overlayW) * 0.5f;

    // Slide-up: starts at H (below screen), ends at target position
    const float targetY  = H - overlayH - 48.0f;
    const float startY   = H + 10.0f;
    const float overlayY = startY + (targetY - startY) * easedPos;

    ImGui::SetNextWindowPos(ImVec2(overlayX, overlayY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(overlayW, overlayH), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.97f * easedAlpha);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.10f, 0.12f, 0.97f));
    ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0.30f, 0.45f, 0.80f, 0.70f * easedAlpha));

    bool open = true;
    if (!ImGui::Begin("##global_ai_overlay", &open,
        ImGuiWindowFlags_NoTitleBar  | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove      | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoFocusOnAppearing))
    {
        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        return;
    }

    // Always on top (above dim background)
    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

    // macOS focus recovery after screen recording / system dialogs
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        !ImGui::IsAnyItemActive() &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        m_FocusInput = true;
    }

    // ── Header ────────────────────────────────────────────────────────────────
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
        int dots = (int)(ImGui::GetTime() * 2.0f) % 4;
        std::string anim = "Думаю" + std::string(dots, '.');
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.60f, 1.0f));
        ImGui::TextUnformatted(anim.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();

    // ── Input field ───────────────────────────────────────────────────────────
    ImGui::Separator();

    // Auto-focus input when overlay finishes opening
    if (m_FocusInput || (m_IsOpen && m_AnimProgress >= 1.0f && !ImGui::IsAnyItemActive()))
    {
        if (m_FocusInput) { ImGui::SetKeyboardFocusHere(); m_FocusInput = false; }
    }
    else if (m_FocusInput)
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
            m_FocusInput  = true;
            Submit(userMsg, host);
        }
    }
    if (busy) ImGui::EndDisabled();

    // Escape closes
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !ImGui::IsAnyItemActive())
        m_IsOpen = false;

    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);

    if (!open) m_IsOpen = false;
}

} // namespace Sandbox
