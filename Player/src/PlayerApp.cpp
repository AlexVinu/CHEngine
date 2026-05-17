// CHEngine Player Runtime
// Loads a .chepak asset archive and runs the startup scene in game mode.
// No editor UI — just render + input + Lua scripts.
#define CHE_INCLUDE_ENTRY_POINT
#include <CHEngine.h>
#include <CHEngine/Scene/SceneSerializer.h>
#include <CHEngine/World/World.h>
#include <CHEngine/Utils/AppPaths.h>
#include <FileSystem/FileSystem.h>
#include <FileSystem/PakFile.h>

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <string>

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
        if (m_World)
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
        auto pakPath = CHEngine::AppPaths::ExecutableDir() / "game.chepak";
        if (std::filesystem::exists(pakPath))
        {
            if (CHEngine::FileSystem::MountPak(pakPath))
                CHE_CORE_INFO("[Player] Mounted {}", pakPath.string());
            else
                CHE_CORE_WARN("[Player] Failed to mount pak: {}", pakPath.string());
        }

        // Read game.json for startup scene
        std::string startupScene = ReadStartupScene();

        // Load shaders needed for game rendering (no editor-only shaders like Grid)
        CHEngine::ResourceManager::Instance().Load<CHEngine::ShaderHandle>(
            "Mesh",   CHEngine::AppPaths::ExecutableDir() / "shaders/mesh.slang");
        CHEngine::ResourceManager::Instance().Load<CHEngine::ShaderHandle>(
            "Sphere", CHEngine::AppPaths::ExecutableDir() / "shaders/sphere_impostor.slang");

        PushLayer(new PlayerLayer(startupScene));
    }

    ~PlayerApp() override = default;

private:
    std::string ReadStartupScene()
    {
        // Try game.json in exe dir (or pak)
        auto gameCfgPath = CHEngine::AppPaths::ExecutableDir() / "game.json";
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
