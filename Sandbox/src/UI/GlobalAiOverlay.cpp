#include "GlobalAiOverlay.h"
#include "SceneViewLayerHost.h"

#include <imgui.h>
#include <imgui_internal.h>

#ifdef CHE_HAS_CURL
#include <curl/curl.h>
#endif

#include <sstream>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <stdexcept>

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
// Script templates (pre-made Lua scripts the AI can assign by name)
// ─────────────────────────────────────────────────────────────────────────────
static const char* kScriptWasd = R"(-- WASD movement + Space to jump
local speed     = 5.0
local jumpForce = 8.0
local gravity   = -20.0
local velY      = 0.0
local grounded  = false
local groundY   = 0.0
local spaceWas  = false

function OnStart(entity)
    groundY = entity:GetPosition().y
end

function OnUpdate(entity, dt)
    local pos = entity:GetPosition()
    if Input.IsKeyDown(Key.W) then pos.z = pos.z - speed * dt end
    if Input.IsKeyDown(Key.S) then pos.z = pos.z + speed * dt end
    if Input.IsKeyDown(Key.A) then pos.x = pos.x - speed * dt end
    if Input.IsKeyDown(Key.D) then pos.x = pos.x + speed * dt end
    local spaceNow = Input.IsKeyDown(Key.Space)
    if spaceNow and not spaceWas and grounded then
        velY = jumpForce; grounded = false
    end
    spaceWas = spaceNow
    velY = velY + gravity * dt
    pos.y = pos.y + velY * dt
    if pos.y <= groundY then pos.y = groundY; velY = 0; grounded = true end
    entity:SetPosition(pos.x, pos.y, pos.z)
end

function OnStop(entity) end
)";

static const char* kScriptRotateY = R"(-- Slow rotation around Y axis
local speed = 45.0  -- degrees per second

function OnStart(entity) end

function OnUpdate(entity, dt)
    local rot = entity:GetRotation()
    entity:SetRotation(rot.x, rot.y + speed * dt, rot.z)
end

function OnStop(entity) end
)";

static const char* kScriptBounce = R"(-- Bounce up and down
local amplitude = 1.5   -- meters
local frequency = 1.2   -- bounces per second
local baseY     = 0.0

function OnStart(entity)
    baseY = entity:GetPosition().y
end

function OnUpdate(entity, dt)
    local t   = Input.IsKeyDown(Key.Space) and 2.0 or 1.0  -- faster when Space held
    local pos = entity:GetPosition()
    local y   = baseY + amplitude * math.abs(math.sin(os.clock() * frequency * t * math.pi))
    entity:SetPosition(pos.x, y, pos.z)
end

function OnStop(entity) end
)";

static const char* kScriptOrbit = R"(-- Orbit around origin
local radius  = 3.0
local speed   = 60.0  -- degrees per second
local angle   = 0.0

function OnStart(entity)
    local pos = entity:GetPosition()
    radius = math.sqrt(pos.x * pos.x + pos.z * pos.z)
    if radius < 0.1 then radius = 3.0 end
    angle = math.atan(pos.z, pos.x) * 180.0 / math.pi
end

function OnUpdate(entity, dt)
    angle = angle + speed * dt
    local rad = angle * math.pi / 180.0
    local pos = entity:GetPosition()
    entity:SetPosition(radius * math.cos(rad), pos.y, radius * math.sin(rad))
end

function OnStop(entity) end
)";

// Returns Lua source for a named template, or "" if unknown
static std::string GetScriptTemplate(const std::string& name)
{
    if (name == "wasd")     return kScriptWasd;
    if (name == "rotate_y") return kScriptRotateY;
    if (name == "bounce")   return kScriptBounce;
    if (name == "orbit")    return kScriptOrbit;
    return "";
}

// ─────────────────────────────────────────────────────────────────────────────
// JSON array parser helpers
// ─────────────────────────────────────────────────────────────────────────────

// Extract each {...} object from "key": [...] array
static std::vector<std::string> ParseJsonActionsArray(const std::string& json)
{
    std::vector<std::string> result;
    size_t pos = json.find("\"actions\"");
    if (pos == std::string::npos) return result;
    pos += 9;
    while (pos < json.size() && json[pos] != '[') ++pos;
    if (pos >= json.size()) return result;
    ++pos;
    int depth = 0; size_t start = std::string::npos;
    for (size_t i = pos; i < json.size(); ++i) {
        if      (json[i] == '{') { if (!depth) start = i; ++depth; }
        else if (json[i] == '}') { --depth; if (!depth && start != std::string::npos) { result.push_back(json.substr(start, i-start+1)); start = std::string::npos; } }
        else if (json[i] == ']' && !depth) break;
    }
    return result;
}

// Parse [x, y, z] or [r, g, b, a] float array from a JSON object
static std::vector<float> ParseJsonVec(const std::string& obj, const std::string& key)
{
    std::vector<float> result;
    std::string searchKey = "\"" + key + "\"";
    size_t pos = obj.find(searchKey);
    if (pos == std::string::npos) return result;
    pos += searchKey.size();
    while (pos < obj.size() && obj[pos] != '[') ++pos;
    if (pos >= obj.size()) return result;
    ++pos;
    std::string num;
    for (; pos < obj.size() && obj[pos] != ']'; ++pos) {
        char c = obj[pos];
        if (c == ',' || c == ']') { if (!num.empty()) { try { result.push_back(std::stof(num)); } catch(...){} num.clear(); } }
        else if (c == '-' || (c >= '0' && c <= '9') || c == '.') num += c;
    }
    if (!num.empty()) { try { result.push_back(std::stof(num)); } catch(...){} }
    return result;
}

static float ParseJsonFloat(const std::string& obj, const std::string& key, float def = 0.0f)
{
    auto v = ParseJsonVec(obj, key); // won't find array, try scalar
    if (!v.empty()) return v[0];
    // Try scalar "key": 1.0
    std::string searchKey = "\"" + key + "\"";
    size_t pos = obj.find(searchKey);
    if (pos == std::string::npos) return def;
    pos += searchKey.size();
    while (pos < obj.size() && (obj[pos] == ' ' || obj[pos] == ':')) ++pos;
    std::string num;
    for (; pos < obj.size(); ++pos) {
        char c = obj[pos];
        if (c == ',' || c == '}' || c == '\n') break;
        if (c == '-' || (c >= '0' && c <= '9') || c == '.') num += c;
    }
    if (!num.empty()) { try { return std::stof(num); } catch(...){} }
    return def;
}

static bool ParseJsonBool(const std::string& obj, const std::string& key, bool def = true)
{
    std::string searchKey = "\"" + key + "\"";
    size_t pos = obj.find(searchKey);
    if (pos == std::string::npos) return def;
    pos += searchKey.size();
    while (pos < obj.size() && (obj[pos] == ' ' || obj[pos] == ':')) ++pos;
    if (pos < obj.size()) {
        if (obj[pos] == 't') return true;
        if (obj[pos] == 'f') return false;
    }
    return def;
}

// ─────────────────────────────────────────────────────────────────────────────
// System prompt — full engine capabilities
// ─────────────────────────────────────────────────────────────────────────────
static const char* kSystemPrompt = R"(You are a command executor for CHEngine 3D editor. Parse user request and return ONLY valid JSON — no markdown, no explanation, nothing else.

FORMAT:
{"message":"<short confirmation same language as user>","actions":[...]}

AVAILABLE ACTIONS (use as many as needed, in logical order):

── CREATE OBJECTS ──
{"type":"create","object":"<kind>","name":"<optional_name>","pos":[x,y,z],"rot":[x,y,z],"scale":[x,y,z]}
  object values: cube, sphere, empty, camera, dir_light, point_light, spot_light
  pos/rot/scale are optional (default: [0,0,0] and [1,1,1])
  name: optional custom name; default names: cube→Cube, sphere→Sphere, camera→Camera, dir_light→Directional Light, point_light→Point Light, spot_light→Spot Light

── TRANSFORM ──
{"type":"set_pos","entity":"<name>","pos":[x,y,z]}
{"type":"set_rot","entity":"<name>","rot":[x,y,z]}
{"type":"set_scale","entity":"<name>","scale":[x,y,z]}

── APPEARANCE ──
{"type":"set_color","entity":"<name>","color":[r,g,b,a]}  (values 0.0-1.0)
{"type":"set_visible","entity":"<name>","visible":false}

── LIGHT ──
{"type":"set_light","entity":"<name>","light_type":"directional|point|spot","color":[r,g,b],"intensity":1.0}

── NAMING / DELETION ──
{"type":"rename","entity":"<old_name>","name":"<new_name>"}
{"type":"delete","entity":"<name>"}

── SCRIPTS ──
{"type":"add_script","entity":"<name>","template":"<wasd|rotate_y|bounce|orbit>"}
  wasd     = WASD movement + Space jump
  rotate_y = continuous Y rotation
  bounce   = up/down bouncing
  orbit    = orbits around origin
  (omit template or leave "" for empty script)

── EDITOR ACTIONS ──
{"type":"select","entity":"<name>"}
{"type":"focus","entity":"<name>"}
{"type":"layout","preset":"script_focus|uv_focus|model_focus|default"}
{"type":"play"}
{"type":"stop"}
{"type":"save"}
{"type":"reset_camera"}
{"type":"set_fov","value":60.0}

IMPORTANT RULES:
1. Create objects BEFORE modifying them by name
2. Positions: Y=0 is ground level; space objects 3-6 units apart
3. Color values are 0.0-1.0 (not 0-255); red=[1,0,0,1], blue=[0,0.5,1,1]
4. Light intensity: 1.0=normal, 2.0=bright, 0.5=dim
5. Layout: use script_focus when working on scripts, model_focus for 3D work, uv_focus for textures
6. Output ONLY the JSON object, nothing else

EXAMPLES:

User: "добавь красный куб"
{"message":"Добавил красный куб","actions":[
  {"type":"create","object":"cube"},
  {"type":"set_color","entity":"Cube","color":[1,0,0,1]}
]}

User: "создай сцену: два шара по бокам и кубик в центре, камера смотрит на куб"
{"message":"Создал сцену с двумя шарами и кубом","actions":[
  {"type":"create","object":"sphere","pos":[-4,0,0]},
  {"type":"create","object":"sphere","pos":[4,0,0]},
  {"type":"create","object":"cube","pos":[0,0,0]},
  {"type":"create","object":"camera","pos":[0,3,8]},
  {"type":"layout","preset":"model_focus"},
  {"type":"focus","entity":"Cube"}
]}

User: "добавь кубик с WASD управлением"
{"message":"Добавил куб с WASD скриптом","actions":[
  {"type":"create","object":"cube"},
  {"type":"add_script","entity":"Cube","template":"wasd"},
  {"type":"layout","preset":"script_focus"}
]}

User: "сделай направленный свет желтоватым и увеличь его яркость"
{"message":"Обновил направленный свет","actions":[
  {"type":"set_light","entity":"Directional Light","light_type":"directional","color":[1,0.95,0.7],"intensity":2.0}
]}

User: "создай сцену: шарик и кубик, шарик вращается, кубик двигается WASD, камера смотрит на куб"
{"message":"Создал сцену с анимацией","actions":[
  {"type":"create","object":"sphere","pos":[-3,0,0]},
  {"type":"add_script","entity":"Sphere","template":"rotate_y"},
  {"type":"create","object":"cube","pos":[3,0,0]},
  {"type":"add_script","entity":"Cube","template":"wasd"},
  {"type":"create","object":"camera","pos":[0,3,8]},
  {"type":"layout","preset":"script_focus"},
  {"type":"focus","entity":"Cube"}
]}

User: "скрой объект Sphere и переименуй Cube в Floor"
{"message":"Скрыл Sphere, переименовал Cube в Floor","actions":[
  {"type":"set_visible","entity":"Sphere","visible":false},
  {"type":"rename","entity":"Cube","name":"Floor"}
]}

User: "сохрани сцену и запусти игру"
{"message":"Сохранил и запустил","actions":[
  {"type":"save"},
  {"type":"play"}
]}

User: "верни стандартный layout"
{"message":"Восстановил стандартный layout","actions":[
  {"type":"layout","preset":"default"}
]}
)";

// ─────────────────────────────────────────────────────────────────────────────
std::string GlobalAiOverlay::BuildRequestBody(const std::string& userMsg) const
{
    std::ostringstream ss;
    ss << "{"
       << "\"model\":\"" << JsonEscape(m_Model) << "\","
       << "\"temperature\":0.1,"
       << "\"max_tokens\":1024,"
       << "\"messages\":["
       <<   "{\"role\":\"system\",\"content\":\"" << JsonEscape(kSystemPrompt) << "\"},"
       <<   "{\"role\":\"user\",\"content\":\"" << JsonEscape(userMsg) << "\"}"
       << "]}";
    return ss.str();
}

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
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        60L);
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
    return "__ERR__CURL not available";
#endif
}

std::string GlobalAiOverlay::ExtractContent(const std::string& httpResponse)
{
    if (httpResponse.rfind("__ERR__", 0) == 0)
        return "__ERR__" + httpResponse.substr(7);

    size_t bodyPos = httpResponse.find("__BODY__");
    if (bodyPos == std::string::npos) return "__ERR__no body";
    const std::string& json = httpResponse.substr(bodyPos + 8);

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

void GlobalAiOverlay::WorkerFn(std::string userMsg)
{
    std::string body    = BuildRequestBody(userMsg);
    std::string httpRaw = CallHttp(body);
    std::string content = ExtractContent(httpRaw);

    std::lock_guard<std::mutex> lock(m_Mutex);
    m_PendingResponse = content;
    m_HasPending      = true;
}

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

    // Extract outermost JSON object from response
    std::string json = raw;
    {
        size_t s = json.find('{'), e = json.rfind('}');
        if (s != std::string::npos && e != std::string::npos && e > s)
            json = json.substr(s, e - s + 1);
    }

    std::string message = JsonField(json, "message");
    if (message.empty()) message = "Готово";

    // Parse and execute each action
    auto actions = ParseJsonActionsArray(json);
    for (auto& act : actions)
    {
        std::string type = JsonField(act, "type");

        if (type == "create")
        {
            std::string object = JsonField(act, "object");
            std::string name   = JsonField(act, "name");
            auto pos   = ParseJsonVec(act, "pos");
            auto rot   = ParseJsonVec(act, "rot");
            auto scale = ParseJsonVec(act, "scale");

            if      (object == "cube")        host.AddCubePrimitive();
            else if (object == "sphere")      host.AddSpherePrimitive();
            else if (object == "empty")       host.AddEmptyEntity();
            else if (object == "camera")      host.AddCameraEntity();
            else if (object == "dir_light")   host.AddDirectionalLight();
            else if (object == "point_light") host.AddPointLight();
            else if (object == "spot_light")  host.AddSpotLight();
            else continue;

            if (!name.empty())
                host.RenameSelectedEntity(name);

            if (pos.size()   >= 3) host.SetSelectedEntityPosition(pos[0],   pos[1],   pos[2]);
            if (rot.size()   >= 3) host.SetSelectedEntityRotation(rot[0],   rot[1],   rot[2]);
            if (scale.size() >= 3) host.SetSelectedEntityScale   (scale[0], scale[1], scale[2]);
        }
        else if (type == "set_pos")
        {
            auto pos = ParseJsonVec(act, "pos");
            std::string ent = JsonField(act, "entity");
            if (pos.size() >= 3) host.SetEntityPositionByName(ent, pos[0], pos[1], pos[2]);
        }
        else if (type == "set_rot")
        {
            auto rot = ParseJsonVec(act, "rot");
            std::string ent = JsonField(act, "entity");
            if (rot.size() >= 3) host.SetEntityRotationByName(ent, rot[0], rot[1], rot[2]);
        }
        else if (type == "set_scale")
        {
            auto sc = ParseJsonVec(act, "scale");
            std::string ent = JsonField(act, "entity");
            if (sc.size() >= 3) host.SetEntityScaleByName(ent, sc[0], sc[1], sc[2]);
        }
        else if (type == "set_color")
        {
            auto col = ParseJsonVec(act, "color");
            std::string ent = JsonField(act, "entity");
            float a = col.size() >= 4 ? col[3] : 1.0f;
            if (col.size() >= 3) host.SetEntityColorByName(ent, col[0], col[1], col[2], a);
        }
        else if (type == "set_visible")
        {
            std::string ent = JsonField(act, "entity");
            bool vis = ParseJsonBool(act, "visible", true);
            host.SetEntityVisibleByName(ent, vis);
        }
        else if (type == "set_light")
        {
            std::string ent       = JsonField(act, "entity");
            std::string lightType = JsonField(act, "light_type");
            auto col              = ParseJsonVec(act, "color");
            float intensity       = ParseJsonFloat(act, "intensity", 1.0f);
            float r = col.size()>0?col[0]:1.0f, g=col.size()>1?col[1]:1.0f, b=col.size()>2?col[2]:1.0f;
            host.SetEntityLightByName(ent, lightType, r, g, b, intensity);
        }
        else if (type == "rename")
        {
            std::string ent  = JsonField(act, "entity");
            std::string name = JsonField(act, "name");
            host.RenameEntityByName(ent, name);
        }
        else if (type == "delete")
        {
            host.DeleteEntityByName(JsonField(act, "entity"));
        }
        else if (type == "add_script")
        {
            std::string ent  = JsonField(act, "entity");
            std::string tmpl = JsonField(act, "template");
            std::string lua  = GetScriptTemplate(tmpl);
            if (!lua.empty())
                host.CreateAndAttachScriptWithContent(ent, lua);
            else
                host.CreateAndAttachScriptToEntityByName(ent);
        }
        else if (type == "select")
        {
            host.SelectEntityByName(JsonField(act, "entity"));
        }
        else if (type == "focus")
        {
            host.FocusOnEntityByName(JsonField(act, "entity"));
        }
        else if (type == "layout")
        {
            host.ApplyLayoutPreset(JsonField(act, "preset"));
        }
        else if (type == "play")  { host.EnterPlayMode();  }
        else if (type == "stop")  { host.StopPlayMode();   }
        else if (type == "save")  { host.SaveScene();      }
        else if (type == "reset_camera") { host.ResetViewportCamera(); }
        else if (type == "set_fov")
        {
            float fov = ParseJsonFloat(act, "value", 45.0f);
            host.SetViewportFovValue(fov);
        }
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
