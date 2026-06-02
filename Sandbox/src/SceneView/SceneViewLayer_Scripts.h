#pragma once

#include <CHEngine.h>

#include <string>

namespace Sandbox { struct EditorContext; }

// Creates/opens .lua files in ScriptEditor and attaches ScriptComponents.
// Lives here (not EditorWorldContext) because it needs ProjectManager + ScriptEditor + Tiling.
namespace SceneViewLayerScripts {

void OpenScriptInEditor(Sandbox::EditorContext& ctx, const std::string& path);
void CreateAndAttachScript(Sandbox::EditorContext& ctx, CHEngine::EntityHandle handle, const std::string& entityName);
void CreateAndAttachScriptWithContent(Sandbox::EditorContext& ctx, const std::string& entityName, const std::string& luaContent);
void CreateAndAttachWorldScript(Sandbox::EditorContext& ctx);
void CreateAndAttachWorldScriptWithContent(Sandbox::EditorContext& ctx, const std::string& luaContent);
void CreateAndAttachScriptToEntityByName(Sandbox::EditorContext& ctx, const std::string& entityName);
void OpenScriptForEntity(Sandbox::EditorContext& ctx, const std::string& entityName);

} // namespace SceneViewLayerScripts
