// CHEngine Player Runtime
#define CHE_INCLUDE_ENTRY_POINT
#include <CHEngine.h>
#include <CHEngine/Scene/SceneSerializer.h>
#include <CHEngine/World/World.h>
#include <CHEngine/Utils/AppPaths.h>
#include <FileSystem/FileSystem.h>
#include <CHEngine/ResourceManager/AssetPack.h>

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using json = nlohmann::json;

// ─────────────────────────────────────────────────────────────────────────────
// PlayerLayer
// ─────────────────────────────────────────────────────────────────────────────
class PlayerLayer : public CHEngine::Layer
{
public:
    explicit PlayerLayer(const std::string& startupScenePath)
        : Layer("PlayerLayer")
    {
        // NOTE: OnAttach() is never called by CHEngine's Application.
        // All initialization must happen in the constructor.
        CHE_CORE_INFO("[Player] Loading scene: {}", startupScenePath);

        CHEngine::SceneSerializer serializer;
        auto scene = serializer.LoadFromFile(startupScenePath);

        if (!scene) {
            CHE_CORE_ERROR("[Player] Scene load failed, using empty scene");
            scene = MakeRef<CHEngine::Scene>();
        } else {
            int objs = 0, cams = 0, meshes = 0;
            scene->ForEach<CHEngine::TagComponent>(
                [&](CHEngine::EntityHandle, const CHEngine::UUID&, CHEngine::TagComponent&) { ++objs; });
            scene->ForEach<CHEngine::CameraComponent>(
                [&](CHEngine::EntityHandle, const CHEngine::UUID&, CHEngine::CameraComponent&) { ++cams; });
            scene->ForEach<CHEngine::MeshComponent>(
                [&](CHEngine::EntityHandle, const CHEngine::UUID&, CHEngine::MeshComponent&) { ++meshes; });
            CHE_CORE_INFO("[Player] Loaded: {} objects, {} cameras, {} meshes", objs, cams, meshes);
        }

        // Fix camera projection for the window size
        auto* win = CHEngine::Application::Get().GetWindow();
        if (win && win->GetWidth() > 0 && win->GetHeight() > 0)
        {
            uint32_t w = win->GetWidth(), h = win->GetHeight();
            scene->ForEach<CHEngine::CameraComponent>(
                [w, h](CHEngine::EntityHandle, const CHEngine::UUID&, CHEngine::CameraComponent& cam) {
                    CHEngine::CameraHelpers::SetViewportSize(cam.Camera, w, h);
                });
        }

        m_World = MakeRef<CHEngine::World>(scene, nullptr);
        m_World->SetState(CHEngine::WorldState::Simulating);
        CHE_CORE_INFO("[Player] World created, Simulating");
    }

    void OnAttach()  override {}  // never called by CHEngine
    void OnDetach()  override { if (m_World) m_World->SetState(CHEngine::WorldState::NONE); }
    void OnEvent(CHEngine::Event&) override {}

    void OnUpdate(CHEngine::Timestep dt) override
    {
        if (!m_World) return;

        auto* win = CHEngine::Application::Get().GetWindow();
        if (win)
        {
            uint32_t w = win->GetWidth(), h = win->GetHeight();
            if (w > 0 && h > 0)
                CHEngine::Application::Get().Render().SetViewportSize(w, h);
        }
        CHEngine::Application::Get().Render().ClearPreSceneCallback();
        m_World->Update(dt);
    }

    // Презент кадра выполняется движком через RenderFactory::PresentToBackbuffer
    // (blit-пасс) — ImGui в рантайме Player не используется, поэтому
    // OnImGuiRender не переопределяется.

private:
    Ref<CHEngine::World> m_World;
};

// ─────────────────────────────────────────────────────────────────────────────
// PlayerApp
// ─────────────────────────────────────────────────────────────────────────────
class PlayerApp : public CHEngine::Application
{
public:
    explicit PlayerApp(const CHEngine::ApplicationConfig& config)
        : CHEngine::Application(config)
    {
        fs::path exeDir      = CHEngine::AppPaths::ExecutableDir();
        fs::path resourceDir = exeDir / "../Resources";
        if (!fs::exists(resourceDir)) resourceDir = exeDir;

        // Mount asset pak
        fs::path pakPath = resourceDir / "game.chepak";
        if (!fs::exists(pakPath)) pakPath = exeDir / "game.chepak";
        if (fs::exists(pakPath))
        {
            if (CHEngine::FileSystem::MountPak(pakPath))
                CHE_CORE_INFO("[Player] Mounted pak: {}", pakPath.string());
            else
                CHE_CORE_WARN("[Player] Failed to mount pak: {}", pakPath.string());
        }

        m_ResourceDir = resourceDir;

        // Load shaders (ResourceManager reads via direct I/O, not pak)
        fs::path shadersDir = exeDir / "shaders";
        CHE_CORE_INFO("[Player] Shaders dir exists: {}", fs::exists(shadersDir));

        auto meshShader = CHEngine::Application::Get().Resources().Load<CHEngine::ShaderHandle>(
            "Mesh",   shadersDir / "mesh.slang");
        auto sphereShader = CHEngine::Application::Get().Resources().Load<CHEngine::ShaderHandle>(
            "Sphere", shadersDir / "sphere_impostor.slang");

        CHE_CORE_INFO("[Player] Mesh shader valid: {}", meshShader.IsValid());
        CHE_CORE_INFO("[Player] Sphere shader valid: {}", sphereShader.IsValid());

        if (meshShader.IsValid())
            CHEngine::Application::Get().Render().SetDefaultMeshShader(meshShader);
        else
            CHE_CORE_ERROR("[Player] Mesh shader FAILED — objects will not render!");

        if (sphereShader.IsValid())
            CHEngine::Application::Get().Render().SetDefaultSphereImpostorShader(sphereShader);

        std::string startupScene = ReadStartupScene();
        CHE_CORE_INFO("[Player] Startup scene: {}", startupScene);

        // Verify file is readable
        auto txt = CHEngine::FileSystem::ReadFileText(fs::path(startupScene));
        CHE_CORE_INFO("[Player] Scene file size: {} bytes", txt.size());

        PushLayer(new PlayerLayer(startupScene));
    }

    ~PlayerApp() override = default;

private:
    fs::path m_ResourceDir;

    std::string ReadStartupScene()
    {
        fs::path exeDir  = CHEngine::AppPaths::ExecutableDir();
        fs::path cfgPath = m_ResourceDir / "game.json";
        if (!fs::exists(cfgPath)) cfgPath = exeDir / "game.json";

        std::string text = CHEngine::FileSystem::ReadFileText(cfgPath);
        if (!text.empty()) {
            try {
                auto j = json::parse(text);
                std::string rel = j.value("startup_scene", "");
                if (!rel.empty())
                    return (exeDir / rel).string();
            } catch (...) {}
        }
        return (exeDir / "Scenes/main.chscene").string();
    }
};

// ─────────────────────────────────────────────────────────────────────────────
namespace {
    // Read the game title from game.json so saves go to a per-game UserDataDir.
    // Stored in a static so the const char* in ApplicationConfig stays valid.
    const char* ResolveGameTitle()
    {
        static std::string s_Title;
        fs::path exeDir = CHEngine::AppPaths::ExecutableDir();
        fs::path cfgPath = exeDir / "../Resources/game.json";
        if (!fs::exists(cfgPath)) cfgPath = exeDir / "game.json";
        std::string text = CHEngine::FileSystem::ReadFileText(cfgPath);
        if (!text.empty()) {
            try {
                auto j = json::parse(text);
                s_Title = j.value("title", "");
            } catch (...) {}
        }
        return s_Title.empty() ? nullptr : s_Title.c_str();
    }
}

CHEngine::Application* CHEngine::CreateApplication(const CHEngine::ApplicationConfig& config)
{
    // Player — headless-runtime без ImGui: презент идёт через blit-пасс рендерера.
    CHEngine::ApplicationConfig playerConfig = config;
    playerConfig.ImGuiEnabled = false;
    playerConfig.AppName      = ResolveGameTitle();
    return new PlayerApp(playerConfig);
}
