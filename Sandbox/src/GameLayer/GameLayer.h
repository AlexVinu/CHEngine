#pragma once

#include <CHEngine/Layer/Layer.h>
#include <CHEngine/World/WorldsList.h>

class GameLayer : public CHEngine::Layer
{
public:
	explicit GameLayer(Ref<CHEngine::WorldsList> worlds);
	void OnUpdate(CHEngine::Timestep dt) override;
	void OnRenderUpdate(CHEngine::Timestep dt) override;
private:
	CHEngine::WorldsList& m_Worlds;
};
