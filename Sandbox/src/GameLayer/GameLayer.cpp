#include "GameLayer.h"

GameLayer::GameLayer(Ref<std::vector<Ref<EditorWorldContext>>> worlds)
	:m_MutualWorlds(worlds)
{

}

void GameLayer::OnUpdate(CHEngine::Timestep dt)
{
	for (const auto& world : *m_MutualWorlds)
	{
		world->RuntimeWorld->Update(dt);
	}
}

void GameLayer::OnImGuiRender()
{

}

void GameLayer::OnEvent(CHEngine::Event& e)
{

}
