#pragma once

#include "AiChatPanel.h"

#include <string>
#include <memory>
#include <functional>

// Forward declare TextEditor (vendor)
class TextEditor;

namespace Sandbox {

// ─────────────────────────────────────────────────────────────────────────────
// ScriptEditorPanel
//
// Редактор Lua-скриптов прямо внутри движка.
//
// Использование:
//   ScriptEditorPanel editor;
//   editor.Open("scripts/player.lua");   // открыть файл
//   editor.Draw();                        // вызывать каждый кадр в OnImGuiRender
//
// Горячие клавиши внутри окна:
//   Ctrl+S        — сохранить
//   Ctrl+Z        — отмена
//   Ctrl+Shift+Z  — повтор
// ─────────────────────────────────────────────────────────────────────────────
class ScriptEditorPanel
{
public:
    // Колбэк: вызывается когда файл сохраняется (путь к файлу)
    using OnSaveCallback = std::function<void(const std::string& path)>;

    ScriptEditorPanel();
    ~ScriptEditorPanel();

    // Открыть Lua-скрипт
    void Open(const std::string& filePath);

    // Открыть шейдер (.slang/.glsl/.hlsl) с HLSL-подсветкой
    void OpenShader(const std::string& filePath);

    // Создать новый пустой скрипт и открыть его
    void NewScript(const std::string& filePath);

    bool IsOpen()  const { return m_IsOpen; }
    bool HasFile() const { return !m_FilePath.empty(); }
    const std::string& GetFilePath() const { return m_FilePath; }

    void SetOnSaveCallback(OnSaveCallback cb) { m_OnSave = std::move(cb); }

    // Доступ к AI-панели (для настройки API key снаружи)
    AiChatPanel& GetAiChat() { return m_AiChat; }

    // Рисовать ImGui окно (вызывать каждый кадр)
    void Draw();

private:
    void Save();
    void LoadFile(const std::string& path);
    std::string GetDefaultScript() const;

    std::unique_ptr<TextEditor> m_Editor;
    std::string  m_FilePath;
    std::string  m_WindowTitle;
    bool         m_IsOpen     = false;
    bool         m_IsDirty    = false;   // несохранённые изменения
    bool         m_ShowAiChat = true;    // показывать AI-панель
    OnSaveCallback m_OnSave;
    AiChatPanel    m_AiChat;
};

} // namespace Sandbox
