#define VK_USE_PLATFORM_WIN32_KHR

#ifndef UINT32_MAX
#define UINT32_MAX	((uint32_t)-1)
#endif

#include <iostream>
#include <set>
#include <array>
#include <algorithm>
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include "Engine.h"

static Engine* s_Instance = nullptr;

void check_vk_result(const VkResult result)
{
	if (result == 0)
	{
		return;
	}

	throw std::runtime_error("[Vulkan] Error: VkResult = " + result);
}

void Engine::CreateVulkanInstance(const std::vector<const char*>& extensions, uint32_t num_extensions)
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

	result = vkCreateInstance(&create_info, nullptr, &m_Instance);
	check_vk_result(result);
}

void Engine::SetupVulkan()
{
	VkResult result;

	// Select GPU
	{
		uint32_t count = 0;
		result = vkEnumeratePhysicalDevices(m_Instance, &count, nullptr);
		check_vk_result(result);

		if (count == 0) 
			throw std::runtime_error("Failed to find a GPU with Vulkan support.");

		std::vector<VkPhysicalDevice> devices(count);

		vkEnumeratePhysicalDevices(m_Instance, &count, devices.data());

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

		m_PhysicalDevice = devices[use_gpu];
	}
	
	// Select Graphics Queue Family
	{
		uint32_t count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &count, nullptr);

		std::vector<VkQueueFamilyProperties> queues(count);
		vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &count, queues.data());

		for (uint32_t i = 0; i < count; i++)
		{
			if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				m_QueueFamily = i;
				break;
			}
		}

		if (m_QueueFamily == UINT32_MAX)
			throw std::runtime_error("Failed to find graphics queue family.");
	}

	// Querying for presentation support and creating the presentation queue
	// (necessary? presentation queue and graphics queue could be identical)

	// Checking for Device Extension Support
	{
		bool available = false;

		uint32_t count;
		vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &count, nullptr);

		std::vector<VkExtensionProperties> device_extensions(count);
		vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &count, device_extensions.data());

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
		queue_info.queueFamilyIndex = m_QueueFamily;
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

		result = vkCreateDevice(m_PhysicalDevice, &create_info, nullptr, &m_Device);
		check_vk_result(result);

		vkGetDeviceQueue(m_Device, m_QueueFamily, 0, &m_Queue);
	}

	// Query Swap Chain support
	{
		VkSurfaceCapabilitiesKHR capabilities;
		result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevice, m_Surface, &capabilities);
		check_vk_result(result);

		uint32_t format_count;
		result = vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &format_count, nullptr);
		check_vk_result(result);

		std::vector<VkSurfaceFormatKHR> formats(format_count);
		result = vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &format_count, formats.data());
		check_vk_result(result);

		uint32_t present_count;
		result = vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, m_Surface, &present_count, nullptr);
		check_vk_result(result);

		std::vector<VkPresentModeKHR> present_modes(present_count);
		result = vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, m_Surface, &present_count, present_modes.data());
		check_vk_result(result);

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

			SDL_Vulkan_GetDrawableSize(m_WindowHandle, &width, &height);
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
		create_info.surface = m_Surface;
		create_info.minImageCount = image_count;
		create_info.imageFormat = selected_format.format;
		create_info.imageColorSpace = selected_format.colorSpace;
		create_info.imageExtent = extent;
		create_info.imageArrayLayers = 1;
		create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		create_info.presentMode = selected_present_mode;

		result = vkCreateSwapchainKHR(m_Device, &create_info, nullptr, &m_SwapChain);
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

	m_WindowHandle = SDL_CreateWindow(m_Specification.Name.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, m_Specification.Width, m_Specification.Height, SDL_WINDOW_VULKAN);

	// Get the names of the Vulkan instance extensions needed to create a surface
	uint32_t num_extensions;
	if (SDL_Vulkan_GetInstanceExtensions(m_WindowHandle, &num_extensions, nullptr) == SDL_FALSE)
		throw std::runtime_error("Error getting Vulkan instance extensions.");

	std::vector<const char*> extensions(num_extensions);
	if (SDL_Vulkan_GetInstanceExtensions(m_WindowHandle, &num_extensions, extensions.data()) == SDL_FALSE)
		throw std::runtime_error("Error getting Vulkan instance extensions.");

	CreateVulkanInstance(extensions, num_extensions);

	if (SDL_Vulkan_CreateSurface(m_WindowHandle, m_Instance, &m_Surface) == SDL_FALSE)
		throw std::runtime_error("Error creating Vulkan Surface.");

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
	vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
	vkDestroySwapchainKHR(m_Device, m_SwapChain, nullptr);
	vkDestroyDevice(m_Device, nullptr);
	vkDestroyInstance(m_Instance, nullptr);

	SDL_DestroyWindow(m_WindowHandle);
	SDL_Quit();
}