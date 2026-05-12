#include "ScriptEditorPanel.h"

#include <TextEditor.h>
#include <imgui.h>

#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>

namespace Sandbox {

// ─────────────────────────────────────────────────────────────────────────────
// Шаблон нового скрипта
// ─────────────────────────────────────────────────────────────────────────────
static const char* kNewScriptTemplate = R"(-- Script: {name}
-- Lua API:
--   entity:GetName()                -> string
--   entity:GetPosition()            -> {x, y, z}
--   entity:SetPosition(x, y, z)
--   entity:GetRotation()            -> {x, y, z}  (Euler degrees)
--   entity:SetRotation(x, y, z)
--   entity:GetScale()               -> {x, y, z}
--   entity:SetScale(x, y, z)
--   entity:GetColor()               -> {r, g, b, a}
--   entity:SetColor(r, g, b, a)
--   Input.IsKeyDown(Key.W)          -> bool
--   Input.IsKeyPressed(Key.Space)   -> bool
--   Log.Info("msg")

function OnStart(entity)
    Log.Info("Script started: " .. entity:GetName())
end

function OnUpdate(entity, dt)
    -- Движение WASD
    local pos = entity:GetPosition()

    if Input.IsKeyDown(Key.W) then pos.z = pos.z - 5.0 * dt end
    if Input.IsKeyDown(Key.S) then pos.z = pos.z + 5.0 * dt end
    if Input.IsKeyDown(Key.A) then pos.x = pos.x - 5.0 * dt end
    if Input.IsKeyDown(Key.D) then pos.x = pos.x + 5.0 * dt end

    entity:SetPosition(pos.x, pos.y, pos.z)
end

function OnStop(entity)
    Log.Info("Script stopped: " .. entity:GetName())
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
                 && ImGui::IsKeyDown(ImGuiKey_LeftCtrl)
                 && ImGui::IsKeyPressed(ImGuiKey_S);
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

} // namespace Sandbox
