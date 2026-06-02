#include "SceneViewLayer_Scripts.h"

#include "EditorContext.h"
#include "ProjectManager.h"
#include "TilingManager.h"

#include <CHEngine/Scene/Components.h>

#include <filesystem>
#include <fstream>
#include <vector>

namespace {

// Show Script Editor in tiling if it is not already visible.
void EnsureScriptEditorVisible(Sandbox::EditorContext& ctx)
{
    Sandbox::TilingManager& tiling = ctx.Tiling;
    if (tiling.IsVisible(Sandbox::PanelID::ScriptEditor))
        return;
    // NB: avoid name `near` — it is a macro in windef.h.
    Sandbox::PanelID nearPanel = tiling.IsVisible(Sandbox::PanelID::ContentBrowser)
        ? Sandbox::PanelID::ContentBrowser
        : Sandbox::PanelID::Viewport;
    tiling.InsertPanel(Sandbox::PanelID::ScriptEditor, nearPanel, Sandbox::DropEdge::Right);
}

std::vector<CHEngine::EntityHandle> FindHandlesByName(CHEngine::Scene* scene, const std::string& name)
{
    std::vector<CHEngine::EntityHandle> result;
    if (!scene || name.empty()) return result;
    scene->ForEach<CHEngine::TagComponent>(
        [&](CHEngine::EntityHandle h, const CHEngine::UUID&, CHEngine::TagComponent& tag) {
            if (scene->GetString(tag.Name) == name)
                result.push_back(h);
        });
    return result;
}

// Resolve a single entity handle by name through the engine's name index
// (GetUUIDByString → handle). Returns an invalid handle if not found.
CHEngine::EntityHandle FindHandleByName(CHEngine::Scene* scene, const std::string& name)
{
    if (!scene || name.empty()) return {};
    return scene->TryGetEntityHandleByUUID(scene->GetUUIDByString(name));
}

// Attach a .lua path to an entity: create ScriptComponent or append to existing one.
void AttachScriptToEntity(CHEngine::Scene* scene, CHEngine::Entity* entity, const std::string& absPath)
{
    if (!entity->HasComponent<CHEngine::ScriptComponent>())
    {
        CHEngine::ScriptsID id = scene->AllocateScripts();
        scene->GetScripts(id)->push_back(CHEngine::ScriptEntry{ absPath, true });
        entity->AddComponent<CHEngine::ScriptComponent>(CHEngine::ScriptComponent{ id });
    }
    else
    {
        entity->PatchComponent<CHEngine::ScriptComponent>(
            [&, scenePtr = scene](CHEngine::ScriptComponent& sc) {
                if (sc.Scripts == CHEngine::INVALID_ID<CHEngine::ScriptsID>) sc.Scripts = scenePtr->AllocateScripts();
                auto* vec = scenePtr->GetScripts(sc.Scripts);
                if (!vec) return;
                for (const auto& e : *vec)
                    if (e.Path == absPath) return;
                vec->push_back(CHEngine::ScriptEntry{ absPath, true });
            });
    }
}

// Return a unique path <scripts>/<base>[_N].lua.
std::filesystem::path UniqueScriptPath(const std::filesystem::path& scriptsDir, const std::string& base)
{
    namespace fs = std::filesystem;
    fs::path scriptPath = scriptsDir / (base + ".lua");
    int suffix = 1;
    while (fs::exists(scriptPath))
        scriptPath = scriptsDir / (base + "_" + std::to_string(suffix++) + ".lua");
    return scriptPath;
}

std::string SanitizeName(const std::string& entityName)
{
    std::string base = entityName.empty() ? "Entity" : entityName;
    for (auto& c : base)
        if (c == ' ' || c == '/' || c == '\\' || c == ':') c = '_';
    return base;
}

} // namespace

void SceneViewLayerScripts::OpenScriptInEditor(Sandbox::EditorContext& ctx, const std::string& path)
{
    namespace fs = std::filesystem;
    std::string absPath = path;
    Ref<ProjectManager> pm = ctx.Projects;
    if (pm->HasProject() && !path.empty() && !fs::path(path).is_absolute())
        absPath = (pm->Current()->RootDir() / path).string();

    ctx.ScriptEditor.Open(absPath);
    EnsureScriptEditorVisible(ctx);
}

void SceneViewLayerScripts::CreateAndAttachScript(Sandbox::EditorContext& ctx,
                                                  CHEngine::EntityHandle handle, const std::string& entityName)
{
    namespace fs = std::filesystem;
    Ref<ProjectManager> pm = ctx.Projects;
    if (!pm->HasProject()) return;

    Project* proj = pm->Current();
    fs::path scriptsDir = proj->ScriptsAbsPath();
    if (!fs::exists(scriptsDir))
        fs::create_directories(scriptsDir);

    fs::path scriptPath = UniqueScriptPath(scriptsDir, SanitizeName(entityName));

    ctx.ScriptEditor.NewScript(scriptPath.string());
    EnsureScriptEditorVisible(ctx);

    // Attach ScriptComponent — store absolute path so LuaScriptSystem can find the file.
    auto scene = ctx.ActiveCtx() ? ctx.ActiveCtx()->EditorScene : nullptr;
    if (!scene) return;
    auto* entity = scene->TryGetEntity(handle);
    if (!entity) return;
    AttachScriptToEntity(scene.get(), entity, scriptPath.string());
}

void SceneViewLayerScripts::CreateAndAttachScriptWithContent(Sandbox::EditorContext& ctx,
                                                             const std::string& entityName, const std::string& luaContent)
{
    namespace fs = std::filesystem;
    Ref<ProjectManager> pm = ctx.Projects;
    if (!pm->HasProject()) return;

    Project* proj = pm->Current();
    fs::path scriptsDir = proj->ScriptsAbsPath();
    if (!fs::exists(scriptsDir)) fs::create_directories(scriptsDir);

    fs::path scriptPath = UniqueScriptPath(scriptsDir, SanitizeName(entityName));

    // Write the Lua content directly
    {
        std::ofstream f(scriptPath);
        if (f) f << luaContent;
    }

    ctx.ScriptEditor.Open(scriptPath.string());
    EnsureScriptEditorVisible(ctx);

    // Attach to the entity by name (not SelectedEntity — avoids attaching to wrong entity)
    auto session = ctx.ActiveCtx();
    auto scene   = session ? session->EditorScene : nullptr;
    if (!scene) return;

    std::string absPath = scriptPath.string();
    auto handles = FindHandlesByName(scene.get(), entityName);
    // Fallback to SelectedEntity if name not found
    if (handles.empty() && scene->IsEntityHandleValid(session->SelectedEntity))
        handles.push_back(session->SelectedEntity);

    for (auto h : handles) {
        auto* e = scene->TryGetEntity(h);
        if (!e) continue;
        AttachScriptToEntity(scene.get(), e, absPath);
    }
}

void SceneViewLayerScripts::CreateAndAttachWorldScript(Sandbox::EditorContext& ctx)
{
    namespace fs = std::filesystem;
    Ref<ProjectManager> pm = ctx.Projects;
    if (!pm->HasProject()) return;

    Project* proj = pm->Current();
    fs::path scriptsDir = proj->ScriptsAbsPath();
    if (!fs::exists(scriptsDir))
        fs::create_directories(scriptsDir);

    fs::path scriptPath = UniqueScriptPath(scriptsDir, "world_script");

    ctx.ScriptEditor.NewWorldScript(scriptPath.string());
    EnsureScriptEditorVisible(ctx);

    auto scene = ctx.ActiveCtx() ? ctx.ActiveCtx()->EditorScene : nullptr;
    if (!scene) return;
    scene->WorldScripts.push_back(CHEngine::ScriptEntry{ scriptPath.string(), true });
}

void SceneViewLayerScripts::CreateAndAttachWorldScriptWithContent(Sandbox::EditorContext& ctx, const std::string& luaContent)
{
    namespace fs = std::filesystem;
    Ref<ProjectManager> pm = ctx.Projects;
    if (!pm->HasProject()) return;

    Project* proj = pm->Current();
    fs::path scriptsDir = proj->ScriptsAbsPath();
    if (!fs::exists(scriptsDir))
        fs::create_directories(scriptsDir);

    fs::path scriptPath = UniqueScriptPath(scriptsDir, "ai_world_script");

    {
        std::ofstream f(scriptPath);
        if (f) f << luaContent;
    }

    ctx.ScriptEditor.Open(scriptPath.string());
    EnsureScriptEditorVisible(ctx);

    auto scene = ctx.ActiveCtx() ? ctx.ActiveCtx()->EditorScene : nullptr;
    if (!scene) return;
    scene->WorldScripts.push_back(CHEngine::ScriptEntry{ scriptPath.string(), true });
}

void SceneViewLayerScripts::CreateAndAttachScriptToEntityByName(Sandbox::EditorContext& ctx, const std::string& entityName)
{
    auto session = ctx.ActiveCtx();
    auto scene = session ? session->EditorScene : nullptr;
    if (!scene) return;

    const CHEngine::EntityHandle found = FindHandleByName(scene.get(), entityName);
    if (!scene->IsEntityHandleValid(found)) return;
    CreateAndAttachScript(ctx, found, entityName);
}

void SceneViewLayerScripts::OpenScriptForEntity(Sandbox::EditorContext& ctx, const std::string& entityName)
{
    auto session = ctx.ActiveCtx();
    auto scene = session ? session->EditorScene : nullptr;
    if (!scene) return;

    const CHEngine::EntityHandle found = FindHandleByName(scene.get(), entityName);
    if (!scene->IsEntityHandleValid(found)) return;
    auto* entity = scene->TryGetEntity(found);
    if (!entity || !entity->HasComponent<CHEngine::ScriptComponent>()) return;

    const auto& sc = entity->GetComponent<CHEngine::ScriptComponent>();
    const auto* scripts = scene->GetScripts(sc.Scripts);
    if (scripts && !scripts->empty())
        OpenScriptInEditor(ctx, (*scripts)[0].Path);
}
