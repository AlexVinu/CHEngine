#include "GameLayer.h"

#include <CHEngine/World/World.h>

GameLayer::GameLayer(Ref<CHEngine::WorldsList> worldsRef)
	: m_Worlds(*worldsRef)
{
}

void GameLayer::OnUpdate(CHEngine::Timestep dt)
{
	m_Worlds.ForEach([dt](CHEngine::World& world) {
		world.Simulate(dt);
		world.PostUpdate();
	});
}

void GameLayer::OnRenderUpdate(CHEngine::Timestep dt)
{
	// Презентация (запись в фрейм-граф) — внутри scope BeginFrameGraph/EndFrameGraph.
	m_Worlds.ForEach([dt](CHEngine::World& world) {
		world.Render(dt);
	});
}
