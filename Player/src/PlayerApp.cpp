// CHEngine Player Runtime
// Loads a .chepak asset archive and runs the startup scene in game mode.
// No editor UI — just render + input + Lua scripts.
#define CHE_INCLUDE_ENTRY_POINT
#include <CHEngine.h>
#include <CHEngine/Scene/SceneSerializer.h>
#include <CHEngine/World/World.h>
#include <CHEngine/Utils/AppPaths.h>
#include <CHEngine/Render/RenderFacade.h>
#include <FileSystem/FileSystem.h>
#include <FileSystem/PakFile.h>

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

using json = nlohmann::json;

// ─────────────────────────────────────────────────────────────────────────────
// PlayerLayer — loads startup scene and runs the game world
// ─────────────────────────────────────────────────────────────────────────────
class PlayerLayer : public CHEngine::Layer
{
public:
    explicit PlayerLayer(const std::string& startupScenePath)
        : Layer("PlayerLayer")
        , m_StartupScenePath(startupScenePath)
    {}

    void OnAttach() override
    {
        // Load scene from pak (or filesystem as fallback)
        CHEngine::SceneSerializer serializer;
        auto scene = serializer.LoadFromFile(m_StartupScenePath);

        if (!scene) {
            CHE_CORE_ERROR("[Player] Failed to load startup scene: {}", m_StartupScenePath);
            // Carry on with an empty scene so the window at least opens
            scene = MakeRef<CHEngine::Scene>();
        }

        m_World = MakeRef<CHEngine::World>(scene);
        m_World->SetState(CHEngine::WorldState::Simulating);
        CHE_CORE_INFO("[Player] Scene loaded: {}", m_StartupScenePath);
    }

    void OnDetach() override
    {
        if (m_World)
            m_World->SetState(CHEngine::WorldState::NONE);
    }

    void OnUpdate(CHEngine::Timestep dt) override
    {
        if (!m_World) return;

        // Set viewport size every frame so RenderSystem knows the render target dimensions.
        // The editor does this via EditorViewport::BeginSceneRender; without it the
        // viewport is zero/undefined and nothing renders (black screen).
        auto* win = CHEngine::Application::Get().GetWindow();
        if (win)
        {
            uint32_t w = win->GetWidth();
            uint32_t h = win->GetHeight();
            if (w > 0 && h > 0)
                CHEngine::RenderFacade::SetViewportSize(w, h);
        }

        // Make sure no editor-only pre-scene callbacks are active
        CHEngine::RenderFacade::ClearPreSceneCallback();

        m_World->Update(dt);
    }

    void OnImGuiRender() override {}

    void OnEvent(CHEngine::Event& /*e*/) override {}

private:
    std::string              m_StartupScenePath;
    Ref<CHEngine::World>     m_World;
};

// ─────────────────────────────────────────────────────────────────────────────
// PlayerApp — minimal Application subclass
// ─────────────────────────────────────────────────────────────────────────────
class PlayerApp : public CHEngine::Application
{
public:
    explicit PlayerApp(const CHEngine::ApplicationConfig& config)
        : CHEngine::Application(config)
    {
        // Mount asset pack if present
        // On macOS inside a .app bundle the binary lives in Contents/MacOS/,
        // resources are in Contents/Resources/. Try both locations.
        fs::path exeDir      = CHEngine::AppPaths::ExecutableDir();
        fs::path resourceDir = exeDir / "../Resources"; // .app bundle layout
        if (!fs::exists(resourceDir)) resourceDir = exeDir; // flat / dev layout

        // Mount asset pack
        fs::path pakPath = resourceDir / "game.chepak";
        if (!fs::exists(pakPath)) pakPath = exeDir / "game.chepak"; // fallback
        if (fs::exists(pakPath))
        {
            if (CHEngine::FileSystem::MountPak(pakPath))
                CHE_CORE_INFO("[Player] Mounted {}", pakPath.string());
            else
                CHE_CORE_WARN("[Player] Failed to mount pak: {}", pakPath.string());
        }

        m_ResourceDir = resourceDir;
        std::string startupScene = ReadStartupScene();

        // Shaders: try pak first (FileSystem falls through), then disk
        fs::path shadersDir = exeDir / "shaders";
        CHEngine::ResourceManager::Instance().Load<CHEngine::ShaderHandle>(
            "Mesh",   shadersDir / "mesh.slang");
        CHEngine::ResourceManager::Instance().Load<CHEngine::ShaderHandle>(
            "Sphere", shadersDir / "sphere_impostor.slang");

        PushLayer(new PlayerLayer(startupScene));
    }

    ~PlayerApp() override = default;

    fs::path m_ResourceDir;

    std::string ReadStartupScene()
    {
        // Try Resources/game.json (bundle layout), then exe dir
        auto gameCfgPath = m_ResourceDir / "game.json";
        if (!fs::exists(gameCfgPath))
            gameCfgPath = CHEngine::AppPaths::ExecutableDir() / "game.json";
        std::string text = CHEngine::FileSystem::ReadFileText(gameCfgPath);

        if (!text.empty()) {
            try {
                auto j = json::parse(text);
                std::string rel = j.value("startup_scene", "");
                if (!rel.empty())
                    return (CHEngine::AppPaths::ExecutableDir() / rel).string();
            } catch (...) {}
        }

        CHE_CORE_WARN("[Player] game.json not found or invalid, using default scene path");
        return (CHEngine::AppPaths::ExecutableDir() / "scenes/main.chscene").string();
    }
};

// ─────────────────────────────────────────────────────────────────────────────
CHEngine::Application* CHEngine::CreateApplication(const CHEngine::ApplicationConfig& config)
{
    return new PlayerApp(config);
}
