#pragma once

#ifdef CHE_PLATFORM_WINDOWS

extern CHEngine::Application* CHEngine::CreateApplication();

int main(int argc, char** argv)
{
	CHEngine::Log::init();
	CHE_CORE_INFO("Init CHEngine");
	CHE_CORE_CRITICAL("WELCOME TO HELL!");

	CHEngine::MemorySystem::Initialize();

	auto app = CHEngine::CreateApplication();
	app->Run();
	delete app;
}

#endif