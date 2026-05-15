#pragma once

#include "EditorWorldContext.h"
#include "ProjectManager.h"

#include <CheStl/MemoryTypes.h>
#include <CHEngine/Layer/Layer.h>

#include <CHEngine/EngineConfig.h>
#include <CHEngine/ResourceManager/ResourceManager.h>
#include <CHEngine/World/World.h>

#include <vector>

class GameLayer : public CHEngine::Layer
{
public:
	GameLayer(Ref<std::vector<Ref<EditorWorldContext>>>);
	void OnUpdate(CHEngine::Timestep dt) override;
	void OnImGuiRender() override;
	void OnEvent(CHEngine::Event& e) override;
private:
	Ref<std::vector<Ref<EditorWorldContext>>> m_MutualWorlds;
};
