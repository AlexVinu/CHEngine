#include "GameLayer.h"

#include <CHEngine/World/World.h>

GameLayer::GameLayer(Ref<CHEngine::WorldsList> worldsRef)
	: m_Worlds(*worldsRef)
{
}

void GameLayer::OnUpdate(CHEngine::Timestep dt)
{
	m_Worlds.ForEach([dt](CHEngine::World& world) {
		world.Update(dt);
	});
}

void GameLayer::OnImGuiRender()
{

}

void GameLayer::OnEvent(CHEngine::Event& e)
{

}
