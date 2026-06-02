// Define before CHEngine.h so the entry-point (main) is compiled only here
#define CHE_INCLUDE_ENTRY_POINT
#include <CHEngine.h>
#include "SceneViewLayer.h"
#include "EditorRenderLayer.h"
#include "EditorInputLayer.h"
#include "GameLayer.h"
#include "ProjectManager.h"
#include "EditorContext.h"

#include <CHEngine/EngineConfig.h>
#include <CHEngine/ResourceManager/ResourceManager.h>
#include <CHEngine/Utils/AppPaths.h>

#include <CHEngine/Input/InputSystem.h>
#include <CHEngine/Scene/ComponentMeta.h>

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

		m_EditorContext = MakeScope<Sandbox::EditorContext>();
		m_Worlds = MakeRef<CHEngine::WorldsList>();
		m_EditorContext->Worlds   = m_Worlds;
		m_EditorContext->Projects = m_ProjectManager;

		PushLayer(new EditorRenderLayer(*m_EditorContext)); 
		PushLayer(new EditorInputLayer(*m_EditorContext)); 
		PushLayer(new SceneViewLayer(*m_EditorContext));   
		PushLayer(new GameLayer(m_Worlds));
	}

	~SandboxApp() = default;

private:
	Ref<ProjectManager> m_ProjectManager;
	Ref<CHEngine::WorldsList> m_Worlds;
	Scope<Sandbox::EditorContext> m_EditorContext;
};

CHEngine::Application* CHEngine::CreateApplication(const CHEngine::ApplicationConfig& config)
{
	return new SandboxApp(config);
}
