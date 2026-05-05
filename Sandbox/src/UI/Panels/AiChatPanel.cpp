#include "AiChatPanel.h"

#include <imgui.h>

#ifdef CHE_HAS_CURL
#include <curl/curl.h>
#endif

#include <sstream>
#include <cstring>
#include <algorithm>
#include <regex>

namespace Sandbox {

// ─────────────────────────────────────────────────────────────────────────────
// CURL helpers
// ─────────────────────────────────────────────────────────────────────────────
#ifdef CHE_HAS_CURL
static size_t CurlWriteCb(char* ptr, size_t size, size_t nmemb, std::string* out)
{
    out->append(ptr, size * nmemb);
    return size * nmemb;
}
#endif

// ─────────────────────────────────────────────────────────────────────────────
// JSON helpers (минимальный, без зависимостей)
// ─────────────────────────────────────────────────────────────────────────────
static std::string JsonEscape(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
AiChatPanel::AiChatPanel()
{
    // Приветственное сообщение
    Message welcome;
    welcome.isUser = false;
    welcome.text   = "Привет! Я помогу написать Lua-скрипты для твоего объекта.\n\n"
                     "Примеры запросов:\n"
                     "• \"При нажатии пробела объект подпрыгивает\"\n"
                     "• \"WASD движение, скорость 5\"\n"
                     "• \"При нажатии U масштаб умножается на 2\"\n"
                     "• \"Объект медленно вращается вокруг Y\"\n\n"
                     "⚙️ Введи API ключ в Settings перед первым запросом.";
    m_Messages.push_back(welcome);

    // Копируем дефолтные значения в буферы settings
    std::strncpy(m_EndpointBuf, m_Endpoint.c_str(), sizeof(m_EndpointBuf) - 1);
    std::strncpy(m_ModelBuf,    m_Model.c_str(),    sizeof(m_ModelBuf)    - 1);
}

AiChatPanel::~AiChatPanel()
{
    if (m_Worker.joinable())
        m_Worker.detach();
}

// ─────────────────────────────────────────────────────────────────────────────
// System prompt с полным описанием Lua API движка
// ─────────────────────────────────────────────────────────────────────────────
std::string AiChatPanel::BuildSystemPrompt()
{
    return R"(You are a Lua script generator for CHEngine — a custom C++ game engine.
Generate ONLY complete, working Lua code. No explanations, no markdown prose outside code blocks.
Wrap code in ```lua ... ``` blocks.

=== CHEngine Lua API ===

Entity (passed as first argument to all functions):
  entity:GetName()                 -> string
  entity:GetPosition()             -> {x, y, z}
  entity:SetPosition(x, y, z)
  entity:GetRotation()             -> {x, y, z}  -- Euler degrees
  entity:SetRotation(x, y, z)
  entity:GetScale()                -> {x, y, z}
  entity:SetScale(x, y, z)
  entity:GetColor()                -> {r, g, b, a}
  entity:SetColor(r, g, b, a)

Input (polling, call every frame in OnUpdate):
  Input.IsKeyDown(key)             -> bool  -- held
  Input.IsKeyPressed(key)          -> bool  -- just pressed this frame
  Input.IsKeyReleased(key)         -> bool  -- just released
  Input.IsMouseButtonDown(btn)     -> bool
  Input.GetMouseX()                -> float
  Input.GetMouseY()                -> float
  Input.GetMouseDeltaX()           -> float
  Input.GetMouseDeltaY()           -> float

Key constants:
  Key.A Key.B Key.C Key.D Key.E Key.F Key.G Key.H Key.I Key.J Key.K Key.L Key.M
  Key.N Key.O Key.P Key.Q Key.R Key.S Key.T Key.U Key.V Key.W Key.X Key.Y Key.Z
  Key.Space  Key.Escape  Key.Enter  Key.Tab  Key.Backspace
  Key.Left   Key.Right   Key.Up     Key.Down
  Key.LeftShift  Key.LeftCtrl
  Key.F1 .. Key.F12

Logging:
  Log.Info("msg")   Log.Warn("msg")   Log.Error("msg")

Script lifecycle:
  function OnStart(entity)         -- called once when Play is pressed
  function OnUpdate(entity, dt)    -- called every frame, dt = delta time seconds
  function OnStop(entity)          -- called when Play stops

=== RULES ===
- Always include all three functions (OnStart, OnUpdate, OnStop)
- CRITICAL: entity is a C++ object — you CANNOT store fields on it like entity._velocity or entity.state. This will CRASH or silently fail.
- Store ALL state in module-level local variables declared OUTSIDE functions:
    local velocityY = 0.0     -- CORRECT: module-level local
    local isGrounded = true   -- CORRECT: module-level local
    entity._velocityY = 0     -- WRONG: will not work, never do this
- dt parameter ensures frame-rate independence: use `value * dt` for smooth motion
- For "jump": use module-level locals for velocityY and isGrounded, apply gravity each frame
- Generate clean, commented code
- Use Input.IsKeyPressed (not IsKeyDown) for single-frame actions like jump
)";
}

// ─────────────────────────────────────────────────────────────────────────────
// Построение тела JSON запроса
// ─────────────────────────────────────────────────────────────────────────────
std::string AiChatPanel::BuildRequestBody(const std::string& userText,
                                           const std::string& editorCode) const
{
    std::string sysPrompt = BuildSystemPrompt();
    std::string userMsg   = userText;
    if (!editorCode.empty() && editorCode.size() > 10)
    {
        userMsg += "\n\nТекущий скрипт в редакторе (можешь улучшить или заменить):\n```lua\n"
               + editorCode + "\n```";
    }

    std::ostringstream ss;
    ss << "{"
       << "\"model\":\"" << JsonEscape(m_Model) << "\","
       << "\"temperature\":0.3,"
       << "\"max_tokens\":2048,"
       << "\"messages\":["
       <<   "{\"role\":\"system\",\"content\":\"" << JsonEscape(sysPrompt) << "\"},"
       <<   "{\"role\":\"user\",\"content\":\"" << JsonEscape(userMsg) << "\"}"
       << "]}";
    return ss.str();
}

// ─────────────────────────────────────────────────────────────────────────────
// HTTP запрос через libcurl
// ─────────────────────────────────────────────────────────────────────────────
std::string AiChatPanel::CallApi(const std::string& body)
{
#ifdef CHE_HAS_CURL
    CURL* curl = curl_easy_init();
    if (!curl) return "__RAW__curl init failed";

    std::string response;
    std::string authHeader   = "Authorization: Bearer " + m_ApiKey;
    std::string contentType  = "Content-Type: application/json";

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, authHeader.c_str());
    headers = curl_slist_append(headers, contentType.c_str());

    curl_easy_setopt(curl, CURLOPT_URL,            m_Endpoint.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,     headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,     body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,  (long)body.size());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  CurlWriteCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        60L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);

    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
        return "__RAW__CURL error: " + std::string(curl_easy_strerror(res));

    // Возвращаем HTTP код + тело для диагностики
    return "__HTTP__" + std::to_string(httpCode) + "__BODY__" + response;
#else
    (void)body;
    return "__RAW__CURL not available";
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// Парсинг ответа: вытаскиваем content из JSON
// ─────────────────────────────────────────────────────────────────────────────
std::string AiChatPanel::ExtractContent(const std::string& json)
{
    // Ищем "content":"..." в ответе OpenAI-совместимого API
    auto findStr = [&](const std::string& key) -> std::string {
        size_t pos = json.find(key);
        if (pos == std::string::npos) return "";
        pos += key.size();
        if (pos >= json.size() || json[pos] != '"') return "";
        ++pos;
        std::string result;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\' && pos + 1 < json.size()) {
                ++pos;
                switch (json[pos]) {
                    case 'n':  result += '\n'; break;
                    case 't':  result += '\t'; break;
                    case 'r':  result += '\r'; break;
                    case '"':  result += '"';  break;
                    case '\\': result += '\\'; break;
                    case '/':  result += '/';  break;
                    case 'u':  {
                        // \uXXXX — unicode escape, декодируем базовые ASCII символы
                        if (pos + 4 < json.size()) {
                            std::string hex = json.substr(pos + 1, 4);
                            unsigned int cp = 0;
                            try { cp = std::stoul(hex, nullptr, 16); } catch (...) {}
                            if (cp < 128) {
                                result += static_cast<char>(cp); // ASCII
                            } else {
                                // UTF-8 encode для символов > 127
                                if (cp < 0x800) {
                                    result += static_cast<char>(0xC0 | (cp >> 6));
                                    result += static_cast<char>(0x80 | (cp & 0x3F));
                                } else {
                                    result += static_cast<char>(0xE0 | (cp >> 12));
                                    result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                                    result += static_cast<char>(0x80 | (cp & 0x3F));
                                }
                            }
                            pos += 4;
                        }
                        break;
                    }
                    default:   result += json[pos]; break;
                }
            } else {
                result += json[pos];
            }
            ++pos;
        }
        return result;
    };

    // Пробуем стандартный формат OpenAI: choices[0].message.content
    std::string content = findStr("\"content\":");
    if (!content.empty()) return content;

    // Fallback: error message
    std::string err = findStr("\"message\":");
    if (!err.empty()) return "Ошибка API: " + err;

    return "Не удалось распарсить ответ: " + json.substr(0, 200);
}

// ─────────────────────────────────────────────────────────────────────────────
// Извлекаем Lua-код из ```lua ... ``` блока
// ─────────────────────────────────────────────────────────────────────────────
std::string AiChatPanel::ExtractCodeBlock(const std::string& text)
{
    // Ищем ```lua ... ``` или ``` ... ```
    size_t start = text.find("```lua");
    if (start == std::string::npos)
        start = text.find("```");
    if (start == std::string::npos)
        return "";

    start = text.find('\n', start);
    if (start == std::string::npos) return "";
    ++start;

    size_t end = text.find("```", start);
    if (end == std::string::npos) return "";

    std::string code = text.substr(start, end - start);
    // Убираем trailing newline
    while (!code.empty() && (code.back() == '\n' || code.back() == '\r'))
        code.pop_back();
    return code;
}

// ─────────────────────────────────────────────────────────────────────────────
// Thread-safe добавление сообщения
// ─────────────────────────────────────────────────────────────────────────────
void AiChatPanel::AppendMessage(bool isUser, const std::string& text, bool isCode)
{
    std::lock_guard<std::mutex> lock(m_MessagesMutex);
    Message msg;
    msg.isUser = isUser;
    msg.text   = text;
    msg.isCode = isCode;
    m_Messages.push_back(msg);
    m_ScrollToBottom = true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Отправка сообщения (из UI thread)
// ─────────────────────────────────────────────────────────────────────────────
void AiChatPanel::SendMessage()
{
    std::string userText = m_InputBuf;
    if (userText.empty() || m_IsLoading.load()) return;

    // Берём текущий код редактора (передаётся через Draw)
    // — уже есть в m_LastEditorCode
    m_InputBuf[0] = '\0';

    AppendMessage(true, userText);
    m_IsLoading.store(true);

    // Запускаем в отдельном потоке
    if (m_Worker.joinable()) m_Worker.join();
    std::string editorCode = m_LastEditorCode;
    m_Worker = std::thread([this, userText, editorCode]() {
        RequestAsync(userText, editorCode);
    });
}

void AiChatPanel::RequestAsync(const std::string& userText, const std::string& editorCode)
{
    if (m_ApiKey.empty()) {
        AppendMessage(false, "⚠️ API ключ не задан. Нажми ⚙️ Settings и введи ключ.");
        m_IsLoading.store(false);
        return;
    }

    std::string body     = BuildRequestBody(userText, editorCode);
    std::string rawResp  = CallApi(body);

    // ── Диагностика: разбираем служебные префиксы ────────────────────────────
    std::string jsonBody;
    if (rawResp.substr(0, 7) == "__RAW__")
    {
        // CURL-ошибка или нет CURL
        AppendMessage(false, "❌ " + rawResp.substr(7));
        m_IsLoading.store(false);
        return;
    }
    else if (rawResp.substr(0, 8) == "__HTTP__")
    {
        // Формат: __HTTP__200__BODY__{json}
        size_t bodyPos = rawResp.find("__BODY__");
        std::string httpCodeStr = rawResp.substr(8, bodyPos - 8);
        jsonBody = (bodyPos != std::string::npos) ? rawResp.substr(bodyPos + 8) : "";
        long httpCode = std::stol(httpCodeStr);

        if (httpCode != 200)
        {
            // Показываем ошибку с телом ответа для диагностики
            AppendMessage(false,
                "❌ HTTP " + httpCodeStr + "\n\nОтвет сервера:\n" + jsonBody.substr(0, 500));
            m_IsLoading.store(false);
            return;
        }
    }
    else
    {
        jsonBody = rawResp;
    }

    std::string content = ExtractContent(jsonBody);
    std::string code    = ExtractCodeBlock(content);

    // Показываем полный ответ
    AppendMessage(false, content, !code.empty());

    // Если есть код — сообщаем через callback
    if (!code.empty() && m_InsertCallback)
    {
        std::string codeToInsert = code;
        // Вызываем из основного потока при следующем Draw
        std::lock_guard<std::mutex> lock(m_PendingMutex);
        m_PendingCode = codeToInsert;
        m_HasPending  = true;
    }

    m_IsLoading.store(false);
}

// ─────────────────────────────────────────────────────────────────────────────
// Рисуем панель
// ─────────────────────────────────────────────────────────────────────────────
void AiChatPanel::Draw(const std::string& currentCode)
{
    m_LastEditorCode = currentCode;

    // Проверяем pending код из worker thread
    {
        std::lock_guard<std::mutex> lock(m_PendingMutex);
        if (m_HasPending && m_InsertCallback)
        {
            m_InsertCallback(m_PendingCode);
            m_PendingCode.clear();
            m_HasPending = false;
        }
    }

    // ── Шапка ────────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.15f, 1.0f));
    ImGui::BeginChild("##ai_header", ImVec2(0, 32), false, ImGuiWindowFlags_NoScrollbar);
    ImGui::SetCursorPosY(7.0f);
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), " AI  Ассистент");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 68);
    if (ImGui::SmallButton("⚙ Settings"))
    {
        m_ShowSettings = true;
        std::strncpy(m_ApiKeyBuf,   m_ApiKey.c_str(),   sizeof(m_ApiKeyBuf)   - 1);
        std::strncpy(m_EndpointBuf, m_Endpoint.c_str(), sizeof(m_EndpointBuf) - 1);
        std::strncpy(m_ModelBuf,    m_Model.c_str(),    sizeof(m_ModelBuf)    - 1);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Separator();

    // ── Settings popup ────────────────────────────────────────────────────────
    if (m_ShowSettings)
    {
        ImGui::SetNextWindowSize(ImVec2(480, 200), ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                                ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        if (ImGui::Begin("AI Settings##aicfg", &m_ShowSettings,
                         ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
        {
            ImGui::TextDisabled("API ключ хранится только в памяти, не сохраняется в файлы");
            ImGui::Separator();

            ImGui::Text("API Key:"); ImGui::SameLine(90);
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##apikey", m_ApiKeyBuf, sizeof(m_ApiKeyBuf),
                             ImGuiInputTextFlags_Password);

            ImGui::Text("Endpoint:"); ImGui::SameLine(90);
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##endpoint", m_EndpointBuf, sizeof(m_EndpointBuf));

            ImGui::Text("Model:"); ImGui::SameLine(90);
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##model", m_ModelBuf, sizeof(m_ModelBuf));

            ImGui::Spacing();
            ImGui::TextDisabled("Kimi: api.moonshot.cn/v1/...  model: moonshot-v1-8k");
            ImGui::Spacing();

            if (ImGui::Button("Apply", ImVec2(100, 0)))
            {
                m_ApiKey   = m_ApiKeyBuf;
                m_Endpoint = m_EndpointBuf;
                m_Model    = m_ModelBuf;
                m_ShowSettings = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(100, 0)))
                m_ShowSettings = false;
        }
        ImGui::End();
    }

    // ── История сообщений ─────────────────────────────────────────────────────
    float inputH = 76.0f;
    ImVec2 histSize = ImVec2(0, ImGui::GetContentRegionAvail().y - inputH - 8);
    ImGui::BeginChild("##ai_history", histSize, false, ImGuiWindowFlags_NoScrollbar);

    {
        std::lock_guard<std::mutex> lock(m_MessagesMutex);

        const float pad    = 8.0f;
        const float rounding = 6.0f;
        const float availW = ImGui::GetContentRegionAvail().x;
        ImDrawList* dl     = ImGui::GetWindowDrawList();

        for (size_t i = 0; i < m_Messages.size(); ++i)
        {
            const auto& msg = m_Messages[i];
            ImGui::PushID((int)i);

            if (msg.isUser)
            {
                // ── Пузырь пользователя — справа ─────────────────────────────
                float maxBubbleW = availW * 0.82f;
                float wrapW      = maxBubbleW - pad * 2.0f;
                ImVec2 textSz    = ImGui::CalcTextSize(msg.text.c_str(), nullptr, false, wrapW);
                float  bubbleW   = std::min(textSz.x + pad * 2.0f + 2.0f, maxBubbleW);
                float  bubbleH   = textSz.y + pad * 2.0f;

                float  posX = availW - bubbleW - 4.0f;
                ImVec2 cursor = ImGui::GetCursorScreenPos();
                ImVec2 bMin   = { cursor.x + posX,         cursor.y };
                ImVec2 bMax   = { bMin.x + bubbleW, bMin.y + bubbleH };

                dl->AddRectFilled(bMin, bMax, IM_COL32(40, 90, 160, 220), rounding);
                ImGui::SetCursorPosX(posX + pad);
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrapW);
                ImGui::TextUnformatted(msg.text.c_str());
                ImGui::PopTextWrapPos();
                // Advance cursor past bubble
                ImGui::SetCursorScreenPos({ cursor.x, bMax.y + 4.0f });
            }
            else
            {
                // ── Пузырь AI — слева ─────────────────────────────────────────
                float maxBubbleW = availW * 0.95f;
                float wrapW      = maxBubbleW - pad * 2.0f;
                ImVec2 textSz    = ImGui::CalcTextSize(msg.text.c_str(), nullptr, false, wrapW);
                float  bubbleH   = textSz.y + pad * 2.0f + (msg.isCode ? 28.0f : 0.0f);

                ImVec2 cursor = ImGui::GetCursorScreenPos();
                ImVec2 bMin   = { cursor.x + 2.0f,                   cursor.y };
                ImVec2 bMax   = { bMin.x + maxBubbleW - 4.0f, bMin.y + bubbleH };

                dl->AddRectFilled(bMin, bMax, IM_COL32(45, 45, 58, 230), rounding);

                // Метка "AI"
                ImGui::SetCursorScreenPos({ bMin.x + pad, bMin.y + pad });
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "AI ");
                ImGui::SameLine(0, 4);

                // Текст сообщения
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrapW - 28.0f);
                ImGui::TextUnformatted(msg.text.c_str());
                ImGui::PopTextWrapPos();

                // Кнопка вставки кода
                if (msg.isCode)
                {
                    ImGui::SetCursorScreenPos({ bMin.x + pad, bMax.y - 24.0f });
                    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.1f, 0.45f, 0.1f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.65f, 0.2f, 1.0f));
                    if (ImGui::SmallButton("  Вставить в редактор  "))
                    {
                        std::string code = ExtractCodeBlock(msg.text);
                        if (!code.empty() && m_InsertCallback)
                            m_InsertCallback(code);
                    }
                    ImGui::PopStyleColor(2);
                }

                ImGui::SetCursorScreenPos({ cursor.x, bMax.y + 4.0f });
            }

            ImGui::Spacing();
            ImGui::PopID();
        }
    }

    // Индикатор загрузки
    if (m_IsLoading.load())
    {
        float t = (float)ImGui::GetTime();
        const char* dots[] = { "AI думает .  ", "AI думает .. ", "AI думает ..." };
        ImGui::TextColored(ImVec4(0.6f, 0.8f, 0.6f, 1.0f),
                           "%s", dots[(int)(t * 2) % 3]);
    }

    if (m_ScrollToBottom)
    {
        ImGui::SetScrollHereY(1.0f);
        m_ScrollToBottom = false;
    }

    ImGui::EndChild();

    ImGui::Separator();

    // ── Поле ввода ────────────────────────────────────────────────────────────
    bool sendNow = false;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 70.0f);

    ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue;
    if (ImGui::InputText("##ai_input", m_InputBuf, sizeof(m_InputBuf), inputFlags))
        sendNow = true;

    ImGui::SameLine();

    bool disabled = m_IsLoading.load() || m_InputBuf[0] == '\0';
    if (disabled) ImGui::BeginDisabled();
    if (ImGui::Button("Отправить", ImVec2(-1, 0)) || sendNow)
        SendMessage();
    if (disabled) ImGui::EndDisabled();
}

} // namespace Sandbox
