#include <iostream>
#include <vector>
#include <SDL2/SDL.h>
#include <vulkan/vulkan.h>

#include "Engine.h"


static VkInstance			g_Instance = VK_NULL_HANDLE;
static VkPhysicalDevice		g_PhysicalDevice = VK_NULL_HANDLE;
static uint32_t				g_QueueFamily = (uint32_t)-1;
static VkDevice				g_Device = VK_NULL_HANDLE;
static VkQueue				g_Queue = VK_NULL_HANDLE;

static Engine* s_Instance = nullptr;

void check_vk_result(VkResult result)
{
	if (result == 0)
	{
		return;
	}

	std::cout << "[vulkan] Error: VkResult = " << result << std::endl;
}

static void SetupVulkan()
{
	VkResult result;

	// Creating Vulkan Instance
	VkInstanceCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.enabledLayerCount = 0;

#ifdef _DEBUG

	// Enable Validation Layers
	const char* validation_layers[] = { "VK_LAYER_KHRONOS_validation" };
	create_info.enabledLayerCount = 1;
	create_info.ppEnabledLayerNames = validation_layers;

#else

	createInfo.enabledLayerCount = 0;

#endif

	result = vkCreateInstance(&create_info, nullptr, &g_Instance);
	check_vk_result(result);

	// Select GPU
	{
		uint32_t count = 0;
		result = vkEnumeratePhysicalDevices(g_Instance, &count, nullptr);
		check_vk_result(result);

		if (count == 0) 
			throw std::runtime_error("Failed to find GPU with Vulkan support.");

		std::vector<VkPhysicalDevice> devices(count);

		vkEnumeratePhysicalDevices(g_Instance, &count, devices.data());

		int use_gpu = 0;

		for (int i = 0; i < (int)count; i++)
		{
			VkPhysicalDeviceProperties properties;

			vkGetPhysicalDeviceProperties(devices[i], &properties);

			if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			{
				use_gpu = i;
				break;
			}
		}

		g_PhysicalDevice = devices[use_gpu];
	}
	
	// Select Graphics Queue Family
	{
		uint32_t count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(g_PhysicalDevice, &count, nullptr);

		std::vector<VkQueueFamilyProperties> queues(count);
		vkGetPhysicalDeviceQueueFamilyProperties(g_PhysicalDevice, &count, queues.data());

		for (uint32_t i = 0; i < count; i++)
		{
			if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				g_QueueFamily = i;
				break;
			}
		}

		if (g_QueueFamily == (uint32_t)-1)
			throw std::runtime_error("Failed to find Graphics Queue Family.");
	}

	// Create Logical Device
	{
		float priority = 1.0f;

		VkDeviceQueueCreateInfo queue_info = {};
		queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_info.queueFamilyIndex = g_QueueFamily;
		queue_info.queueCount = 1;
		queue_info.pQueuePriorities = &priority;

		VkPhysicalDeviceFeatures features = {};

		VkDeviceCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		create_info.pQueueCreateInfos = &queue_info;
		create_info.queueCreateInfoCount = 1;
		create_info.pEnabledFeatures = &features;

#ifdef _DEBUG
		create_info.enabledLayerCount = 1;
		create_info.ppEnabledLayerNames = validation_layers;
#else
		create_info.enabledLayerCount = 0;
#endif

		result = vkCreateDevice(g_PhysicalDevice, &create_info, nullptr, &g_Device);
		check_vk_result(result);

		vkGetDeviceQueue(g_Device, g_QueueFamily, 0, &g_Queue);
	}
}

Engine::Engine(const EngineSpecification& specification)
	: m_Specification(specification)
{
	s_Instance = this;

	Init();
}

Engine::~Engine()
{
	Shutdown();

	s_Instance = nullptr;
}

Engine& Engine::Get()
{
	return *s_Instance;
}

void Engine::Init()
{
	SDL_Init(SDL_INIT_VIDEO);

	m_WindowHandle = SDL_CreateWindow(m_Specification.Name.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, m_Specification.Width, m_Specification.Height, SDL_WINDOW_SHOWN);

	SetupVulkan();
}

void Engine::Run()
{
	m_Running = true;

	SDL_Surface* screenSurface = SDL_GetWindowSurface(m_WindowHandle);

	SDL_FillRect(screenSurface, NULL, SDL_MapRGB(screenSurface->format, 0, 255, 0));
	SDL_UpdateWindowSurface(m_WindowHandle);

	SDL_Event e; 
	bool quit = false; 
	
	while (!quit)
	{
		while (SDL_PollEvent(&e))
		{
			if (e.type == SDL_QUIT)
				quit = true;
		}
	}
}

void Engine::Shutdown()
{
	vkDestroyDevice(g_Device, nullptr);
	vkDestroyInstance(g_Instance, nullptr);

	SDL_DestroyWindow(m_WindowHandle);
	SDL_Quit();
}