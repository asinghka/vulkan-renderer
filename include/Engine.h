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
	void CreateVulkanInstance();
	void SetupSDL();
	void CreateSDLSurface();
	void SetupVulkan();
	void CreateVulkanRenderPass();
	void CreateVulkanGraphicsPipeline();
	void CreateVulkanFramebuffers();
	void CreateVulkanCommandPool();
	void CreateVulkanCommandBuffer();
	void CreateVulkanSyncObjects();

	void RecordCommandBuffer(VkCommandBuffer buffer, uint32_t image_index);
	void RenderFrame();

private:
	EngineSpecification m_Specification;
	SDL_Window* m_WindowHandle = nullptr;

	std::vector<const char*> m_SDLExtensions;
	uint32_t m_SDLExtensionCount;

	VkInstance				m_Instance = VK_NULL_HANDLE;
	VkSurfaceKHR			m_Surface = VK_NULL_HANDLE;
	VkPhysicalDevice		m_PhysicalDevice = VK_NULL_HANDLE;
	uint32_t				m_QueueFamily = UINT32_MAX;
	VkDevice				m_Device = VK_NULL_HANDLE;
	VkQueue					m_Queue = VK_NULL_HANDLE;
	VkSwapchainKHR			m_Swapchain = VK_NULL_HANDLE;
	VkFormat				m_SwapchainImageFormat;
	VkExtent2D				m_SwapchainExtent;
	VkPipelineLayout		m_PipelineLayout = VK_NULL_HANDLE;
	VkRenderPass			m_Renderpass = VK_NULL_HANDLE;
	VkPipeline				m_Pipeline = VK_NULL_HANDLE;
	VkCommandPool			m_CommandPool = VK_NULL_HANDLE;
	VkCommandBuffer			m_CommandBuffer = VK_NULL_HANDLE;
	VkSemaphore				m_ImageAvailableSemaphore = VK_NULL_HANDLE;
	VkSemaphore				m_RenderFinishedSemaphore = VK_NULL_HANDLE;
	VkFence					m_InFlightFence = VK_NULL_HANDLE;

	std::vector<VkImage>		m_SwapchainImages;
	std::vector<VkImageView>	m_SwapchainImageViews;
	std::vector<VkFramebuffer>	m_SwapchainFramebuffers;
};