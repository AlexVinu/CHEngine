// Define before CHEngine.h so the entry-point (main) is compiled only here
#define CHE_INCLUDE_ENTRY_POINT
#include <CHEngine.h>
#include "SceneViewLayer.h"
#include "GameLayer.h"
#include "ProjectManager.h"

#include <CHEngine/EngineConfig.h>
#include <CHEngine/ResourceManager/ResourceManager.h>
#include <CHEngine/Utils/AppPaths.h>

#include <CHEngine/Input/InputSystem.h>

#include <Render/UniformBlocks.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/matrix.hpp>
#include <string>
#include <cstring>

class SandboxApp : public CHEngine::Application
{
public:
	SandboxApp(const CHEngine::ApplicationConfig& config)
		: CHEngine::Application(config)
	{
		// Загрузка биндов: каждый JSON в config/keybindings/ = отдельный контекст.
		auto* is = CHEngine::Application::Get().InputSystem();
		is->LoadFromDirectory(CHEngine::AppPaths::ExecutableDir() / "config/keybindings");
		is->PushContext("Editor");

		// Try to restore the last used project so the editor can start immediately.
		const std::string lastProj = CHEngine::EngineConfig::LoadLastProject();
		m_ProjectManager = MakeRef<ProjectManager>();
		if (!lastProj.empty())
			m_ProjectManager->Open(lastProj);

		CHEngine::Application::Get().Resources().Load<CHEngine::ShaderHandle>("Flat", CHEngine::AppPaths::ExecutableDir() / "shaders/flat.slang");
		CHEngine::Application::Get().Resources().Load<CHEngine::ShaderHandle>("Neon", CHEngine::AppPaths::ExecutableDir() / "shaders/neon.slang");

		m_MutualWorlds = MakeRef<std::vector<Ref<EditorWorldContext>>>();
		m_MutualWorlds->reserve(16);
		// Scene editor (default)
		PushLayer(new SceneViewLayer(m_MutualWorlds, m_ProjectManager));
		PushLayer(new GameLayer(m_MutualWorlds));
		// Old cube demo (uncomment to use instead):
		// PushLayer(new ExampleLayer());
	}

	~SandboxApp() = default;

private:
	Ref<std::vector<Ref<EditorWorldContext>>> m_MutualWorlds;
	Ref<ProjectManager>                       m_ProjectManager;
};

CHEngine::Application* CHEngine::CreateApplication(const CHEngine::ApplicationConfig& config)
{
	return new SandboxApp(config);
}
