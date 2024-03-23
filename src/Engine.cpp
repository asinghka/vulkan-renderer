#define VK_USE_PLATFORM_WIN32_KHR
#define UINT32_MAX	((uint32_t)-1)

#include <iostream>
#include <vector>
#include <set>
#include <algorithm>
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include "Engine.h"

static VkInstance			g_Instance = VK_NULL_HANDLE;
static VkSurfaceKHR			g_Surface = VK_NULL_HANDLE;
static VkPhysicalDevice		g_PhysicalDevice = VK_NULL_HANDLE;
static uint32_t				g_QueueFamily = UINT32_MAX;
static VkDevice				g_Device = VK_NULL_HANDLE;
static VkQueue				g_Queue = VK_NULL_HANDLE;
static VkSwapchainKHR		g_SwapChain = VK_NULL_HANDLE;

static Engine* s_Instance = nullptr;

void check_vk_result(const VkResult result)
{
	if (result == 0)
	{
		return;
	}

	throw std::runtime_error("[Vulkan] Error: VkResult = " + result);
}

static void CreateVulkanInstance(std::vector<const char*> extensions, uint32_t num_extensions)
{
	VkResult result;

	// Creating Vulkan Instance
	VkInstanceCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.enabledLayerCount = 0;
	create_info.enabledExtensionCount = num_extensions;
	create_info.ppEnabledExtensionNames = extensions.data();

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
}

static void SetupVulkan(SDL_Window* window)
{
	VkResult result;

	// Select GPU
	{
		uint32_t count = 0;
		result = vkEnumeratePhysicalDevices(g_Instance, &count, nullptr);
		check_vk_result(result);

		if (count == 0) 
			throw std::runtime_error("Failed to find a GPU with Vulkan support.");

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

		if (g_QueueFamily == UINT32_MAX)
			throw std::runtime_error("Failed to find graphics queue family.");
	}

	// Querying for presentation support and creating the presentation queue
	// (necessary? presentation queue and graphics queue could be identical)

	// Checking for Device Extension Support
	{
		bool available = false;

		uint32_t count;
		vkEnumerateDeviceExtensionProperties(g_PhysicalDevice, nullptr, &count, nullptr);

		std::vector<VkExtensionProperties> device_extensions(count);
		vkEnumerateDeviceExtensionProperties(g_PhysicalDevice, nullptr, &count, device_extensions.data());

		for (const auto& extension : device_extensions)
		{
			if (strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME))
			{
				available = true;
				break;
			}
		}

		if (!available)
			throw std::runtime_error("Failed to find all required device extensions.");
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

		const char* device_extension[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

		VkDeviceCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		create_info.pQueueCreateInfos = &queue_info;
		create_info.queueCreateInfoCount = 1;
		create_info.pEnabledFeatures = &features;
		create_info.enabledExtensionCount = 1;
		create_info.ppEnabledExtensionNames = device_extension;

#ifdef _DEBUG
		const char* validation_layers[] = { "VK_LAYER_KHRONOS_validation" };
		create_info.enabledLayerCount = 1;
		create_info.ppEnabledLayerNames = validation_layers;
#else
		create_info.enabledLayerCount = 0;
#endif

		result = vkCreateDevice(g_PhysicalDevice, &create_info, nullptr, &g_Device);
		check_vk_result(result);

		vkGetDeviceQueue(g_Device, g_QueueFamily, 0, &g_Queue);
	}

	// Query Swap Chain support
	{
		VkSurfaceCapabilitiesKHR capabilities;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_PhysicalDevice, g_Surface, &capabilities);

		uint32_t format_count;
		vkGetPhysicalDeviceSurfaceFormatsKHR(g_PhysicalDevice, g_Surface, &format_count, nullptr);

		std::vector<VkSurfaceFormatKHR> formats(format_count);
		vkGetPhysicalDeviceSurfaceFormatsKHR(g_PhysicalDevice, g_Surface, &format_count, formats.data());

		uint32_t present_count;
		vkGetPhysicalDeviceSurfacePresentModesKHR(g_PhysicalDevice, g_Surface, &present_count, nullptr);

		std::vector<VkPresentModeKHR> present_modes(present_count);
		vkGetPhysicalDeviceSurfacePresentModesKHR(g_PhysicalDevice, g_Surface, &present_count, present_modes.data());

		if (formats.empty() || present_modes.empty())
		{
			throw std::runtime_error("Error: Surface Formats or Presentation Modes empty.");
		}

		// Choose Swap Chain Surface Format
		VkSurfaceFormatKHR selected_format = formats[0];

		for (const auto& format : formats)
		{
			if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				selected_format = format;
			}
		}

		// Choose Present Mode
		VkPresentModeKHR selected_present_mode = VK_PRESENT_MODE_FIFO_KHR;

		for (const auto& present_mode : present_modes)
		{
			if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				selected_present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
			}
		}

		// Choose Swap Extent
		VkExtent2D extent = {};

		if (capabilities.currentExtent.width != UINT32_MAX)
		{
			extent = capabilities.currentExtent;
		}
		else
		{
			int width, height;

			SDL_Vulkan_GetDrawableSize(window, &width, &height);
			extent.width = static_cast<uint32_t>(width);
			extent.height = static_cast<uint32_t>(height);

			extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
		}

		// Create Swap Chain
		uint32_t image_count = capabilities.minImageCount + 1;
		
		if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount)
		{
			image_count = capabilities.maxImageCount;
		}
		
		VkSwapchainCreateInfoKHR create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		create_info.surface = g_Surface;
		create_info.minImageCount = image_count;
		create_info.imageFormat = selected_format.format;
		create_info.imageColorSpace = selected_format.colorSpace;
		create_info.imageExtent = extent;
		create_info.imageArrayLayers = 1;
		create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		create_info.presentMode = selected_present_mode;

		result = vkCreateSwapchainKHR(g_Device, &create_info, nullptr, &g_SwapChain);
		check_vk_result(result);
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

	m_WindowHandle = SDL_CreateWindow(m_Specification.Name.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, m_Specification.Width, m_Specification.Height, SDL_WINDOW_VULKAN);

	// Get the names of the Vulkan instance extensions needed to create a surface with
	uint32_t num_extensions;
	if (SDL_Vulkan_GetInstanceExtensions(m_WindowHandle, &num_extensions, nullptr) == SDL_FALSE)
		throw std::runtime_error("Error getting Vulkan instance extensions.");

	std::vector<const char*> extensions(num_extensions);
	if (SDL_Vulkan_GetInstanceExtensions(m_WindowHandle, &num_extensions, extensions.data()) == SDL_FALSE)
		throw std::runtime_error("Error getting Vulkan instance extensions.");

	CreateVulkanInstance(extensions, num_extensions);

	if (SDL_Vulkan_CreateSurface(m_WindowHandle, g_Instance, &g_Surface) == SDL_FALSE)
		throw std::runtime_error("Error creating Vulkan Surface.");

	SetupVulkan(m_WindowHandle);
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
	vkDestroySurfaceKHR(g_Instance, g_Surface, nullptr);
	vkDestroySwapchainKHR(g_Device, g_SwapChain, nullptr);
	vkDestroyDevice(g_Device, nullptr);
	vkDestroyInstance(g_Instance, nullptr);

	SDL_DestroyWindow(m_WindowHandle);
	SDL_Quit();
}