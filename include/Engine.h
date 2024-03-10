#pragma once

#include <string>

struct SDL_Window;

struct EngineSpecification
{
	std::string Name = "Vulkan Renderer";
	uint32_t Width = 1600;
	uint32_t Height = 900;
};

class Engine
{
public:
	Engine(const EngineSpecification& engineSpecification = EngineSpecification());
	~Engine();

	static Engine& Get();

	void Run();

	SDL_Window* GetWindowHandle() const { return m_WindowHandle; };

private:
	void Init();
	void Shutdown();
private:
	EngineSpecification m_Specification;
	SDL_Window* m_WindowHandle = nullptr;

	bool m_Running = false;
};