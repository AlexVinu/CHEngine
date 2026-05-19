#include "ScriptEditorPanel.h"

#include <CHEngine/Application.h>

#include <TextEditor.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>

namespace Sandbox {

// ─────────────────────────────────────────────────────────────────────────────
// Шаблон нового скрипта
// ─────────────────────────────────────────────────────────────────────────────
static const char* kNewScriptTemplate = R"(-- Entity Script: {name}
-- Callbacks (все опциональны):
--   OnStart(entity, world)
--   OnUpdate(entity, world, dt)
--   OnStop(entity, world)
--
-- entity API: GetName/GetUUID/IsValid/Destroy
--             HasTransform/HasColor/HasLight/HasMesh/HasRigidBody/HasScript/HasLifetime/HasVisibility/HasCamera
--             AddTransform/AddColor/AddVisibility/AddLifetime/AddLight (+ Remove*)
--             GetPosition/SetPosition, GetRotation/SetRotation, GetScale/SetScale
--             GetColor/SetColor, GetVisibility/SetVisibility
--             GetLifetime/SetLifetime, GetLightIntensity/SetLightIntensity, GetLightColor/SetLightColor
-- world API:  FindByName, FindByUUID, SpawnEntity, DestroyEntity, ForEach
-- modules:    Input.IsKeyDown/IsKeyPressed, Key.W/A/S/D/Space/..., Log.Info/Warn/Error

function OnStart(entity, world)
    Log.Info("Script started: " .. entity:GetName())
end

function OnUpdate(entity, world, dt)
    -- Движение WASD
    local pos = entity:GetPosition()

    if Input.IsKeyDown(Key.W) then pos.z = pos.z - 5.0 * dt end
    if Input.IsKeyDown(Key.S) then pos.z = pos.z + 5.0 * dt end
    if Input.IsKeyDown(Key.A) then pos.x = pos.x - 5.0 * dt end
    if Input.IsKeyDown(Key.D) then pos.x = pos.x + 5.0 * dt end

    entity:SetPosition(pos.x, pos.y, pos.z)
end

function OnStop(entity, world)
    Log.Info("Script stopped: " .. entity:GetName())
end
)";

static const char* kNewWorldScriptTemplate = R"(-- World Script: {name}
-- Callbacks (все опциональны):
--   OnStart(world)
--   OnUpdate(world, dt)
--   OnStop(world)
--
-- world API: FindByName(name), FindByUUID(uuid_str), SpawnEntity(name) -> uuid_str,
--            DestroyEntity(entity), ForEach(function(entity) ... end)
-- modules:   Input.*, Key.*, Log.*

function OnStart(world)
    Log.Info("World script started")
end

function OnUpdate(world, dt)
    -- Глобальная игровая логика, спавн, ввод, и т.д.
    -- Пример:
    -- if Input.IsKeyPressed(Key.Space) then
    --     local uuid = world:SpawnEntity("Bullet")
    --     Log.Info("Spawned " .. uuid)
    -- end
end

function OnStop(world)
    Log.Info("World script stopped")
end
)";

// ─────────────────────────────────────────────────────────────────────────────
ScriptEditorPanel::ScriptEditorPanel()
    : m_Editor(std::make_unique<TextEditor>())
{
    // Lua подсветка синтаксиса
    auto lang = TextEditor::LanguageDefinition::Lua();
    m_Editor->SetLanguageDefinition(lang);

    // Тёмная тема
    m_Editor->SetPalette(TextEditor::GetDarkPalette());
    m_Editor->SetShowWhitespaces(false);
    m_Editor->SetTabSize(4);

    // AI-чат: когда AI генерирует код — вставляем в редактор
    m_AiChat.SetInsertCallback([this](const std::string& code) {
        m_Editor->SetText(code);
        m_IsDirty = true;
    });
}

ScriptEditorPanel::~ScriptEditorPanel() = default;

void ScriptEditorPanel::SetEntityContext(std::string name, bool hasScript, std::string scriptPath,
                                          std::function<void()> onAddScript,
                                          std::function<void()> onEditScript)
{
    m_HasEntityCtx   = true;
    m_CtxEntityName  = std::move(name);
    m_CtxHasScript   = hasScript;
    m_CtxScriptPath  = std::move(scriptPath);
    m_CtxOnAddScript = std::move(onAddScript);
    m_CtxOnEditScript= std::move(onEditScript);
}

void ScriptEditorPanel::ClearEntityContext()
{
    m_HasEntityCtx = false;
    m_CtxEntityName.clear();
    m_CtxOnAddScript  = nullptr;
    m_CtxOnEditScript = nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
void ScriptEditorPanel::Open(const std::string& filePath)
{
    if (filePath.empty()) return;
    m_Editor->SetLanguageDefinition(TextEditor::LanguageDefinition::Lua());
    m_FilePath = filePath;
    LoadFile(filePath);
    m_IsOpen = true;
    m_IsDirty = false;
    m_WindowTitle = "Script: " + std::filesystem::path(filePath).filename().string();
}

void ScriptEditorPanel::OpenShader(const std::string& filePath)
{
    if (filePath.empty()) return;
    m_Editor->SetLanguageDefinition(TextEditor::LanguageDefinition::HLSL());
    m_FilePath = filePath;
    LoadFile(filePath);
    m_IsOpen = true;
    m_IsDirty = false;
    m_WindowTitle = "Shader: " + std::filesystem::path(filePath).filename().string();
}

void ScriptEditorPanel::NewScript(const std::string& filePath)
{
    // Создаём директорию если нет
    auto dir = std::filesystem::path(filePath).parent_path();
    if (!dir.empty() && !std::filesystem::exists(dir))
        std::filesystem::create_directories(dir);

    // Генерируем шаблон с именем файла
    std::string name = std::filesystem::path(filePath).stem().string();
    std::string content = kNewScriptTemplate;
    auto pos = content.find("{name}");
    if (pos != std::string::npos)
        content.replace(pos, 6, name);

    // Пишем файл
    std::ofstream f(filePath);
    if (f) f << content;

    Open(filePath);
    m_IsDirty = false;
}

void ScriptEditorPanel::NewWorldScript(const std::string& filePath)
{
    auto dir = std::filesystem::path(filePath).parent_path();
    if (!dir.empty() && !std::filesystem::exists(dir))
        std::filesystem::create_directories(dir);

    std::string name = std::filesystem::path(filePath).stem().string();
    std::string content = kNewWorldScriptTemplate;
    auto pos = content.find("{name}");
    if (pos != std::string::npos)
        content.replace(pos, 6, name);

    std::ofstream f(filePath);
    if (f) f << content;

    Open(filePath);
    m_IsDirty = false;
}

// ─────────────────────────────────────────────────────────────────────────────
void ScriptEditorPanel::LoadFile(const std::string& path)
{
    if (!std::filesystem::exists(path))
    {
        m_Editor->SetText("-- File not found: " + path);
        return;
    }
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    m_Editor->SetText(ss.str());
}

void ScriptEditorPanel::Save()
{
    if (m_FilePath.empty()) return;

    auto dir = std::filesystem::path(m_FilePath).parent_path();
    if (!dir.empty() && !std::filesystem::exists(dir))
        std::filesystem::create_directories(dir);

    std::ofstream f(m_FilePath);
    if (f)
    {
        f << m_Editor->GetText();
        m_IsDirty = false;
        if (m_OnSave) m_OnSave(m_FilePath);
    }
}

std::string ScriptEditorPanel::GetDefaultScript() const
{
    return kNewScriptTemplate;
}

// ─────────────────────────────────────────────────────────────────────────────
void ScriptEditorPanel::Draw()
{
    if (!m_IsOpen) return;

    // Настройки окна
    ImGui::SetNextWindowSize(ImVec2(760, 560), ImGuiCond_FirstUseEver);

    std::string winId = m_WindowTitle + "###ScriptEditor";
    ImGuiWindowFlags flags = ImGuiWindowFlags_MenuBar;
    if (m_IsDirty) flags |= ImGuiWindowFlags_UnsavedDocument;

    if (!ImGui::Begin(winId.c_str(), &m_IsOpen, flags))
    {
        ImGui::End();
        return;
    }

    // ── Горячие клавиши ─────────────────────────────────────────────────────
    bool wantSave = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
                 && CHEngine::Application::Get().InputSystem()->Triggered("Editor.Script.Save");
    if (wantSave) Save();

    // ── Menu bar ─────────────────────────────────────────────────────────────
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Save", "Ctrl+S", false, !m_FilePath.empty()))
                Save();
            if (ImGui::MenuItem("Reload"))
                LoadFile(m_FilePath);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Undo", "Ctrl+Z",  false, m_Editor->CanUndo())) m_Editor->Undo();
            if (ImGui::MenuItem("Redo", "Ctrl+Y",  false, m_Editor->CanRedo())) m_Editor->Redo();
            ImGui::Separator();
            if (ImGui::MenuItem("Copy",  "Ctrl+C")) m_Editor->Copy();
            if (ImGui::MenuItem("Cut",   "Ctrl+X")) m_Editor->Cut();
            if (ImGui::MenuItem("Paste", "Ctrl+V")) m_Editor->Paste();
            ImGui::Separator();
            if (ImGui::MenuItem("Select All", "Ctrl+A")) m_Editor->SelectAll();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View"))
        {
            if (ImGui::MenuItem("Dark theme"))  m_Editor->SetPalette(TextEditor::GetDarkPalette());
            if (ImGui::MenuItem("Light theme")) m_Editor->SetPalette(TextEditor::GetLightPalette());
            ImGui::Separator();
            ImGui::MenuItem("AI Chat", nullptr, &m_ShowAiChat);
            ImGui::EndMenu();
        }

        // Кнопка AI прямо в menu bar
        ImGui::SameLine(ImGui::GetWindowWidth() - (m_ShowAiChat ? 320.0f : 200.0f));
        if (ImGui::SmallButton(m_ShowAiChat ? "Hide AI" : "Show AI"))
            m_ShowAiChat = !m_ShowAiChat;

        // Статус
        auto cpos = m_Editor->GetCursorPosition();
        ImGui::SameLine(ImGui::GetWindowWidth() - 140.0f);
        ImGui::TextDisabled("Ln %d, Col %d | Lua", cpos.mLine + 1, cpos.mColumn + 1);

        ImGui::EndMenuBar();
    }

    // ── Путь к файлу ─────────────────────────────────────────────────────────
    if (!m_FilePath.empty())
    {
        ImGui::TextDisabled("%s", m_FilePath.c_str());
        ImGui::Separator();
    }

    // ── Split layout: редактор | AI чат ───────────────────────────────────────
    ImVec2 avail = ImGui::GetContentRegionAvail();

    if (m_ShowAiChat)
    {
        // Редактор занимает 60%, AI — 40%
        const float aiW     = std::max(280.0f, avail.x * 0.38f);
        const float editorW = avail.x - aiW - 6.0f; // 6px разделитель

        // Редактор (левая часть)
        ImGui::BeginChild("##editor_pane", ImVec2(editorW, avail.y), false,
                          ImGuiWindowFlags_NoScrollbar);
        m_Editor->Render("##lua_editor", ImGui::GetContentRegionAvail());
        if (m_Editor->IsTextChanged()) m_IsDirty = true;
        ImGui::EndChild();

        ImGui::SameLine();

        // Вертикальный разделитель
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 p = ImGui::GetCursorScreenPos();
            dl->AddLine(ImVec2(p.x, p.y), ImVec2(p.x, p.y + avail.y),
                        IM_COL32(80, 80, 100, 200), 1.0f);
        }
        ImGui::SameLine();

        // AI-чат (правая часть)
        ImGui::BeginChild("##ai_pane", ImVec2(aiW - 4.0f, avail.y), false,
                          ImGuiWindowFlags_NoScrollbar);
        m_AiChat.Draw(m_Editor->GetText());
        ImGui::EndChild();
    }
    else
    {
        // Только редактор
        m_Editor->Render("##lua_editor", avail);
        if (m_Editor->IsTextChanged()) m_IsDirty = true;
    }

    ImGui::End();
}

void ScriptEditorPanel::DrawInPanel()
{
    // In tiling mode the window pos/size is set externally via SetNextWindowPos/Size.
    // We open the window unconditionally and show a hint when no file is loaded.
    if (!ImGui::Begin("Script Editor"))
    {
        ImGui::End();
        return;
    }

    if (!m_IsOpen || m_FilePath.empty())
    {
        ImVec2 avail = ImGui::GetContentRegionAvail();

        if (m_HasEntityCtx && !m_CtxEntityName.empty())
        {
            // ── Entity context hint ──────────────────────────────────────────
            const char* btnLabel = m_CtxHasScript ? "Edit Script" : "Add Script";
            ImVec2 btnSz = ImVec2(130, 0);

            std::string entityMsg = "Selected: " + m_CtxEntityName;
            ImVec2 szMsg = ImGui::CalcTextSize(entityMsg.c_str());
            float totalH = szMsg.y + 10.0f + ImGui::GetFrameHeight();

            ImGui::SetCursorPos(ImVec2((avail.x - szMsg.x) * 0.5f,
                                       (avail.y - totalH) * 0.5f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.75f, 0.78f, 1.0f));
            ImGui::TextUnformatted(entityMsg.c_str());
            ImGui::PopStyleColor();

            ImGui::SetCursorPosX((avail.x - btnSz.x) * 0.5f);
            if (ImGui::Button(btnLabel, btnSz))
            {
                if (m_CtxHasScript && m_CtxOnEditScript)
                    m_CtxOnEditScript();
                else if (!m_CtxHasScript && m_CtxOnAddScript)
                    m_CtxOnAddScript();
            }
        }
        else
        {
            // ── Default hint ─────────────────────────────────────────────────
            const char* line1 = "No script open";
            const char* line2 = "Open a .lua file from the Content Browser";
            ImVec2 sz1 = ImGui::CalcTextSize(line1);
            ImVec2 sz2 = ImGui::CalcTextSize(line2);
            float totalH = sz1.y + 6.0f + sz2.y;

            ImGui::SetCursorPos(ImVec2((avail.x - sz1.x) * 0.5f,
                                       (avail.y - totalH) * 0.5f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.72f, 1.0f));
            ImGui::TextUnformatted(line1);
            ImGui::PopStyleColor();

            ImGui::SetCursorPosX((avail.x - sz2.x) * 0.5f);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.52f, 1.0f));
            ImGui::TextUnformatted(line2);
            ImGui::PopStyleColor();
        }

        ImGui::End();
        return;
    }

    // File is loaded — reuse Draw() logic but we're already inside Begin/End.
    // Just render the editor contents directly (same as Draw body after Begin).
    // Hot keys
    bool wantSave = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
                 && CHEngine::Application::Get().InputSystem()->Triggered("Editor.Script.Save");
    if (wantSave) Save();

    // Editor + AI pane (reuse same split logic)
    ImVec2 avail = ImGui::GetContentRegionAvail();
    const float aiW    = m_ShowAiChat ? ImMax(200.0f, avail.x * 0.30f) : 0.0f;
    const float editorW = avail.x - (m_ShowAiChat ? aiW + 4.0f : 0.0f);

    if (m_ShowAiChat)
    {
        ImGui::BeginChild("##se_ed", ImVec2(editorW, avail.y), false,
                          ImGuiWindowFlags_NoScrollbar);
        m_Editor->Render("##lua_ed", ImGui::GetContentRegionAvail());
        if (m_Editor->IsTextChanged()) m_IsDirty = true;
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("##se_ai", ImVec2(aiW - 4.0f, avail.y), false,
                          ImGuiWindowFlags_NoScrollbar);
        m_AiChat.Draw(m_Editor->GetText());
        ImGui::EndChild();
    }
    else
    {
        m_Editor->Render("##lua_ed", avail);
        if (m_Editor->IsTextChanged()) m_IsDirty = true;
    }

    ImGui::End();
}

} // namespace Sandbox
