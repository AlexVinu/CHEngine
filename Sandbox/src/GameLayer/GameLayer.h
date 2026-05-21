#pragma once

#include <CHEngine/Layer/Layer.h>
#include <CHEngine/World/WorldsList.h>

class GameLayer : public CHEngine::Layer
{
public:
	explicit GameLayer(CHEngine::WorldsList& worlds);
	void OnUpdate(CHEngine::Timestep dt) override;
	void OnImGuiRender() override;
	void OnEvent(CHEngine::Event& e) override;
private:
	CHEngine::WorldsList& m_Worlds;
};
