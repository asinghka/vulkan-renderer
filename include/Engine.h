#pragma once

#include <string>
#include <vector>
#include <vulkan/vulkan.h>

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
	void CreateVulkanInstance(const std::vector<const char*>& extensions, uint32_t num_extensions);
	void SetupVulkan();

private:
	EngineSpecification m_Specification;
	SDL_Window* m_WindowHandle = nullptr;

	VkInstance				m_Instance = VK_NULL_HANDLE;
	VkSurfaceKHR			m_Surface = VK_NULL_HANDLE;
	VkPhysicalDevice		m_PhysicalDevice = VK_NULL_HANDLE;
	uint32_t				m_QueueFamily = UINT32_MAX;
	VkDevice				m_Device = VK_NULL_HANDLE;
	VkQueue					m_Queue = VK_NULL_HANDLE;
	VkSwapchainKHR			m_SwapChain = VK_NULL_HANDLE;


	bool m_Running = false;
};