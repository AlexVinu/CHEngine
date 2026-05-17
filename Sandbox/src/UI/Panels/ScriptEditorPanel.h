#pragma once

#include "AiChatPanel.h"

#include <string>
#include <memory>
#include <functional>
#include <optional>

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

    // То же для world-скриптов (использует kNewWorldScriptTemplate с world-callbacks).
    void NewWorldScript(const std::string& filePath);

    bool IsOpen()  const { return m_IsOpen; }
    bool HasFile() const { return !m_FilePath.empty(); }
    const std::string& GetFilePath() const { return m_FilePath; }

    void SetOnSaveCallback(OnSaveCallback cb) { m_OnSave = std::move(cb); }

    // Доступ к AI-панели (для настройки API key снаружи)
    AiChatPanel& GetAiChat() { return m_AiChat; }

    // Контекст выбранного entity — показывается в DrawInPanel когда нет открытого файла
    void SetEntityContext(std::string name, bool hasScript, std::string scriptPath,
                          std::function<void()> onAddScript,
                          std::function<void()> onEditScript);
    void ClearEntityContext();

    // Рисовать ImGui окно (вызывать каждый кадр)
    void Draw();

    // Tiling mode: always show panel regardless of m_IsOpen.
    // Shows "Open a script from Content Browser" when no file is loaded.
    void DrawInPanel();

private:
    void Save();
    void LoadFile(const std::string& path);
    std::string GetDefaultScript() const;

    std::unique_ptr<TextEditor> m_Editor;
    std::string  m_FilePath;
    std::string  m_WindowTitle;
    bool         m_IsOpen     = false;
    bool         m_IsDirty    = false;
    bool         m_ShowAiChat = true;
    OnSaveCallback m_OnSave;
    AiChatPanel    m_AiChat;

    // Entity context for hint UI in DrawInPanel
    bool         m_HasEntityCtx  = false;
    std::string  m_CtxEntityName;
    bool         m_CtxHasScript  = false;
    std::string  m_CtxScriptPath;
    std::function<void()> m_CtxOnAddScript;
    std::function<void()> m_CtxOnEditScript;
};

} // namespace Sandbox
