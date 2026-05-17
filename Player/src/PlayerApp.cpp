// CHEngine Player Runtime
// Loads a .chepak asset archive and runs the startup scene in game mode.
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
// PlayerLayer
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
        // Diagnostics
        CHE_CORE_INFO("[Player] Pak mounted: {}", CHEngine::FileSystem::IsPakMounted());
        CHE_CORE_INFO("[Player] Loading scene from: {}", m_StartupScenePath);

        CHEngine::SceneSerializer serializer;
        auto scene = serializer.LoadFromFile(m_StartupScenePath);

        if (!scene) {
            CHE_CORE_ERROR("[Player] Failed to load scene: {}", m_StartupScenePath);
            scene = MakeRef<CHEngine::Scene>();
        } else {
            // Count objects for diagnostics
            int objCount = 0, camCount = 0, meshCount = 0;
            scene->ForEach<CHEngine::TagComponent>(
                [&](CHEngine::EntityHandle, const CHEngine::UUID&, CHEngine::TagComponent&) { ++objCount; });
            scene->ForEach<CHEngine::CameraComponent>(
                [&](CHEngine::EntityHandle, const CHEngine::UUID&, CHEngine::CameraComponent&) { ++camCount; });
            scene->ForEach<CHEngine::MeshComponent>(
                [&](CHEngine::EntityHandle, const CHEngine::UUID&, CHEngine::MeshComponent&) { ++meshCount; });
            CHE_CORE_INFO("[Player] Scene loaded: {} objects, {} cameras, {} meshes",
                          objCount, camCount, meshCount);
        }

        // Update camera projection for the window size
        auto* win = CHEngine::Application::Get().GetWindow();
        if (win && win->GetWidth() > 0 && win->GetHeight() > 0)
        {
            float w = (float)win->GetWidth();
            float h = (float)win->GetHeight();
            scene->ForEach<CHEngine::CameraComponent>(
                [w, h](CHEngine::EntityHandle, const CHEngine::UUID&, CHEngine::CameraComponent& cam) {
                    cam.Camera.SetViewportSize((uint32_t)w, (uint32_t)h);
                });
        }

        m_World = MakeRef<CHEngine::World>(scene);
        m_World->SetState(CHEngine::WorldState::Simulating);
    }

    void OnDetach() override
    {
        if (m_World)
            m_World->SetState(CHEngine::WorldState::NONE);
    }

    void OnUpdate(CHEngine::Timestep dt) override
    {
        if (!m_World) return;

        // Set viewport size every frame — RenderSystem requires this before rendering.
        // Without it EnsureGPUResources() returns false and nothing is drawn.
        auto* win = CHEngine::Application::Get().GetWindow();
        if (win)
        {
            uint32_t w = win->GetWidth();
            uint32_t h = win->GetHeight();
            if (w > 0 && h > 0)
                CHEngine::RenderFacade::SetViewportSize(w, h);
        }

        // Remove editor-only pre-scene callbacks (grid, etc.)
        CHEngine::RenderFacade::ClearPreSceneCallback();

        m_World->Update(dt);
    }

    void OnImGuiRender() override
    {
        // ── Present the rendered frame to the screen ──────────────────────────
        // The engine renders into an offscreen LDR texture. The ONLY way to get
        // pixels onto the window surface is via ImGui::Image — same as the editor.
        uint64_t texID = CHEngine::RenderFacade::GetViewportColorTexID();
        if (texID == 0)
        {
            // Texture not ready yet — draw magenta so we know ImGui layer IS running
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
            ImGui::SetNextWindowBgAlpha(1.0f);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.6f, 0.0f, 0.6f, 1.0f));
            ImGui::Begin("##no_tex", nullptr,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
            ImGui::TextUnformatted("Render texture not ready (texID=0)");
            ImGui::End();
            ImGui::PopStyleColor();
            return;
        }

        ImGuiIO& io = ImGui::GetIO();
        ImVec2   displaySize = io.DisplaySize;

        // Fullscreen, no padding, no decoration
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(displaySize, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.0f);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        ImGui::Begin("##game_output", nullptr,
            ImGuiWindowFlags_NoTitleBar      |
            ImGuiWindowFlags_NoResize        |
            ImGuiWindowFlags_NoMove          |
            ImGuiWindowFlags_NoScrollbar     |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNav);

        ImGui::PopStyleVar(2);

        // UV: Metal renders top-left = (0,0); OpenGL renders top-left = (0,1)
        const bool isMetal = (CHEngine::Application::Get().GetRenderAPIType()
                               == CHEngine::ERenderAPI::METAL);
        ImVec2 uv0 = isMetal ? ImVec2(0, 0) : ImVec2(0, 1);
        ImVec2 uv1 = isMetal ? ImVec2(1, 1) : ImVec2(1, 0);

        ImGui::Image(static_cast<ImTextureID>(texID), displaySize, uv0, uv1);

        ImGui::End();
    }

    void OnEvent(CHEngine::Event& /*e*/) override {}

private:
    std::string          m_StartupScenePath;
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
        // Locate Resources dir — handles both .app bundle and flat/dev layout
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

        // ── Load shaders ──────────────────────────────────────────────────────
        fs::path shadersDir = exeDir / "shaders";

        CHE_CORE_INFO("[Player] Loading shaders from: {}", shadersDir.string());
        CHE_CORE_INFO("[Player] Shaders dir exists: {}", fs::exists(shadersDir));

        auto meshShader = CHEngine::ResourceManager::Instance().Load<CHEngine::ShaderHandle>(
            "Mesh",   shadersDir / "mesh.slang");
        auto sphereShader = CHEngine::ResourceManager::Instance().Load<CHEngine::ShaderHandle>(
            "Sphere", shadersDir / "sphere_impostor.slang");

        CHE_CORE_INFO("[Player] Mesh shader valid: {}", meshShader.IsValid());
        CHE_CORE_INFO("[Player] Sphere shader valid: {}", sphereShader.IsValid());

        if (meshShader.IsValid())
            CHEngine::RenderFacade::SetDefaultMeshShader(meshShader);
        else
            CHE_CORE_ERROR("[Player] Mesh shader FAILED — objects will not render!");

        if (sphereShader.IsValid())
            CHEngine::RenderFacade::SetDefaultSphereImpostorShader(sphereShader);

        std::string startupScene = ReadStartupScene();
        CHE_CORE_INFO("[Player] Startup scene: {}", startupScene);

        PushLayer(new PlayerLayer(startupScene));
    }

    ~PlayerApp() override = default;

private:
    fs::path m_ResourceDir;

    std::string ReadStartupScene()
    {
        fs::path exeDir = CHEngine::AppPaths::ExecutableDir();

        // Try Resources/game.json (.app bundle), then exe dir (dev/flat)
        fs::path cfgPath = m_ResourceDir / "game.json";
        if (!fs::exists(cfgPath)) cfgPath = exeDir / "game.json";

        std::string text = CHEngine::FileSystem::ReadFileText(cfgPath);
        if (!text.empty())
        {
            try {
                auto j = json::parse(text);
                std::string rel = j.value("startup_scene", "");
                if (!rel.empty())
                    return (exeDir / rel).string();
            } catch (...) {}
        }

        CHE_CORE_WARN("[Player] game.json not found, using default scene path");
        return (exeDir / "scenes/main.chscene").string();
    }
};

// ─────────────────────────────────────────────────────────────────────────────
CHEngine::Application* CHEngine::CreateApplication(const CHEngine::ApplicationConfig& config)
{
    return new PlayerApp(config);
}
