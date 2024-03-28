#include <iostream>
#include <set>
#include <array>
#include <algorithm>
#include <fstream>
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include "Engine.h"


#define VK_USE_PLATFORM_WIN32_KHR

#ifndef UINT32_MAX
#define UINT32_MAX	((uint32_t)-1)
#endif


static Engine* s_Instance = nullptr;

void check_vk_result(const VkResult result)
{
	if (result == 0)
	{
		return;
	}

	throw std::runtime_error("[Vulkan] Error: VkResult = " + result);
}

static std::vector<char> ReadFile(const std::string& filename)
{
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open())
	{
		throw std::runtime_error("Failed to open file.");
	}

	auto file_size = file.tellg();
	file.seekg(0);

	std::vector<char> buffer(file_size);

	file.seekg(0);
	file.read(buffer.data(), file_size);

	file.close();

	return buffer;
}

void Engine::CreateVulkanInstance()
{
	VkResult result;

	// Creating Vulkan Instance
	VkInstanceCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.enabledLayerCount = 0;
	create_info.enabledExtensionCount = m_SDLExtensionCount;
	create_info.ppEnabledExtensionNames = m_SDLExtensions.data();

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
		create_info.preTransform = capabilities.currentTransform;
		create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		create_info.clipped = VK_TRUE;
		create_info.oldSwapchain = VK_NULL_HANDLE;

		m_SwapchainImageFormat = selected_format.format;
		m_SwapchainExtent = extent;

		result = vkCreateSwapchainKHR(m_Device, &create_info, nullptr, &m_Swapchain);
		check_vk_result(result);

		uint32_t swapchain_image_count;
		result = vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &swapchain_image_count, nullptr);
		check_vk_result(result);

		m_SwapchainImages.resize(swapchain_image_count);
		result = vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &swapchain_image_count, m_SwapchainImages.data());
		check_vk_result(result);

		m_SwapchainImageViews.resize(swapchain_image_count);
	}

	// Create Image Views
	{
		for (size_t i = 0; i < m_SwapchainImages.size(); i++)
		{
			VkImageViewCreateInfo create_info = {};
			create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			create_info.image = m_SwapchainImages[i];
			create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
			create_info.format = m_SwapchainImageFormat;
			
			create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

			create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			create_info.subresourceRange.baseMipLevel = 0;
			create_info.subresourceRange.levelCount = 1;
			create_info.subresourceRange.baseArrayLayer = 0;
			create_info.subresourceRange.layerCount = 1;

			result = vkCreateImageView(m_Device, &create_info, nullptr, &m_SwapchainImageViews[i]);
			check_vk_result(result);
		}
	}
}

void Engine::SetupVulkanGraphicsPipeline()
{
	VkResult result;

	// Read Shader Binaries
	auto vert_shader_code = ReadFile("assets/shaders/vert.spv");
	auto frag_shader_code = ReadFile("assets/shaders/frag.spv");

	// Vertex Shader Module Create Info
	VkShaderModule vert_shader_module;
	VkShaderModule frag_shader_module;
	{
		VkShaderModuleCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		create_info.codeSize = vert_shader_code.size();
		create_info.pCode = reinterpret_cast<const uint32_t*> (vert_shader_code.data());
		result = vkCreateShaderModule(m_Device, &create_info, nullptr, &vert_shader_module);
		check_vk_result(result);
	}

	// Fragment Shader Module Create Info
	{
		VkShaderModuleCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		create_info.codeSize = frag_shader_code.size();
		create_info.pCode = reinterpret_cast<const uint32_t*> (frag_shader_code.data());
		result = vkCreateShaderModule(m_Device, &create_info, nullptr, &frag_shader_module);
		check_vk_result(result);
	}

	// Vertex Shader Stage Create Info
	std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages;
	{
		VkPipelineShaderStageCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
		create_info.module = vert_shader_module;
		create_info.pName = "main";

		shader_stages[0] = create_info;
	}

	// Fragment Shader Stage Create Info
	{
		VkPipelineShaderStageCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
		create_info.module = frag_shader_module;
		create_info.pName = "main";

		shader_stages[1] = create_info;
	}

	// Vertex Input Create Info
	VkPipelineVertexInputStateCreateInfo vertex_input = {};
	vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input.vertexBindingDescriptionCount = 0;
	vertex_input.pVertexBindingDescriptions = nullptr;
	vertex_input.vertexAttributeDescriptionCount = 0;
	vertex_input.pVertexAttributeDescriptions = nullptr;

	// Input Assembly Create Info
	VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
	input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	input_assembly.primitiveRestartEnable = VK_FALSE;

	/* Viewport and Scissor (as dynamic part of the pipeline)
	std::vector<VkDynamicState> dynamic_states = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

	VkPipelineDynamicStateCreateInfo dynamic_state = {};
	dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
	dynamic_state.pDynamicStates = dynamic_states.data();

	VkPipelineViewportStateCreateInfo viewport_state = {};
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.viewportCount = 1;
	viewport_state.scissorCount = 1;
	*/

	// Viewport and Scissors (as static part of the pipeline)
	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)m_SwapchainExtent.width;
	viewport.height = (float)m_SwapchainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = {0, 0};
	scissor.extent = m_SwapchainExtent;

	VkPipelineViewportStateCreateInfo viewport_state = {};
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = &viewport;
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = &scissor;

	// Rasterizer Create Info
	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f;
	rasterizer.depthBiasClamp = 0.0f;
	rasterizer.depthBiasSlopeFactor = 0.0f;

	// Multisampling Create Info (Disabled)
	VkPipelineMultisampleStateCreateInfo multisample = {};
	multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample.sampleShadingEnable = VK_FALSE;
	multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisample.minSampleShading = 1.0f;
	multisample.pSampleMask = nullptr;
	multisample.alphaToCoverageEnable = VK_FALSE;
	multisample.alphaToOneEnable = VK_FALSE;

	// Color Blending Attachment State and Create Info
	VkPipelineColorBlendAttachmentState color_blend_attachment = {};
	color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	color_blend_attachment.blendEnable = VK_FALSE;
	color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
	color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo color_blend = {};
	color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blend.logicOpEnable = VK_FALSE;
	color_blend.logicOp = VK_LOGIC_OP_COPY;
	color_blend.attachmentCount = 1;
	color_blend.pAttachments = &color_blend_attachment;
	color_blend.blendConstants[0] = 0.0f;
	color_blend.blendConstants[1] = 0.0f;
	color_blend.blendConstants[2] = 0.0f;
	color_blend.blendConstants[3] = 0.0f;

	// Create Pipeline Layout
	{
		VkPipelineLayoutCreateInfo pipeline_layout = {};
		pipeline_layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipeline_layout.setLayoutCount = 0;
		pipeline_layout.pSetLayouts = nullptr;
		pipeline_layout.pPushConstantRanges = 0;
		pipeline_layout.pPushConstantRanges = nullptr;
		
		result = vkCreatePipelineLayout(m_Device, &pipeline_layout, nullptr, &m_PipelineLayout);
		check_vk_result(result);
	}
	
	vkDestroyShaderModule(m_Device, vert_shader_module, nullptr);
	vkDestroyShaderModule(m_Device, frag_shader_module, nullptr);
}

void Engine::SetupSDL()
{
	SDL_Init(SDL_INIT_VIDEO);

	m_WindowHandle = SDL_CreateWindow(m_Specification.Name.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, m_Specification.Width, m_Specification.Height, SDL_WINDOW_VULKAN);

	// Get the names of the Vulkan instance extensions needed to create a surface
	if (SDL_Vulkan_GetInstanceExtensions(m_WindowHandle, &m_SDLExtensionCount, nullptr) == SDL_FALSE)
		throw std::runtime_error("Error getting Vulkan instance extensions.");

	m_SDLExtensions.resize(m_SDLExtensionCount);
	if (SDL_Vulkan_GetInstanceExtensions(m_WindowHandle, &m_SDLExtensionCount, m_SDLExtensions.data()) == SDL_FALSE)
		throw std::runtime_error("Error getting Vulkan instance extensions.");
}

void Engine::CreateSDLSurface()
{
	if (SDL_Vulkan_CreateSurface(m_WindowHandle, m_Instance, &m_Surface) == SDL_FALSE)
	{
		throw std::runtime_error("Error creating Vulkan Surface.");
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
	SetupSDL();
	CreateVulkanInstance();
	CreateSDLSurface();
	SetupVulkan();
	SetupVulkanGraphicsPipeline();
}

void Engine::Run()
{
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
	for (VkImageView image_view : m_SwapchainImageViews)
	{
		vkDestroyImageView(m_Device, image_view, nullptr);
	}

	vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);
	vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
	vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
	vkDestroyDevice(m_Device, nullptr);
	vkDestroyInstance(m_Instance, nullptr);

	SDL_DestroyWindow(m_WindowHandle);
	SDL_Quit();
}