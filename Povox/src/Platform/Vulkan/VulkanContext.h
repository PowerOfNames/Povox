#pragma once

#include "Povox/Renderer/GraphicsContext.h"

#include "Povox/Core/Core.h"
#include "VulkanDevice.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <glm/glm.hpp>

struct GLFWwindow;

namespace Povox {

	struct UniformBufferObject
	{
		alignas(16) glm::mat4 ModelMatrix;
		alignas(16) glm::mat4 ViewMatrix;
		alignas(16) glm::mat4 ProjectionMatrix;
	};	

	struct VertexData
	{
		glm::vec3 Position;
		glm::vec3 Color;
		glm::vec2 TexCoord;

		static VkVertexInputBindingDescription getBindingDescription()
		{
			VkVertexInputBindingDescription vertexBindingDescription{};
			vertexBindingDescription.binding = 0;
			vertexBindingDescription.stride = sizeof(VertexData);
			vertexBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return vertexBindingDescription;
		}

		static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions()
		{
			std::array<VkVertexInputAttributeDescription, 3> vertexAttributeDescriptions{};
			vertexAttributeDescriptions[0].binding = 0;
			vertexAttributeDescriptions[0].location = 0;
			vertexAttributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			vertexAttributeDescriptions[0].offset = offsetof(VertexData, Position);

			vertexAttributeDescriptions[1].binding = 0;
			vertexAttributeDescriptions[1].location = 1;
			vertexAttributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			vertexAttributeDescriptions[1].offset = offsetof(VertexData, Color);

			vertexAttributeDescriptions[2].binding = 0;
			vertexAttributeDescriptions[2].location = 2;
			vertexAttributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
			vertexAttributeDescriptions[2].offset = offsetof(VertexData, TexCoord);

			return vertexAttributeDescriptions;
		}
	};

	class VulkanContext : public GraphicsContext
	{
	public:
		VulkanContext(GLFWwindow* windowHandle);

		virtual void Init() override;
		virtual void DrawFrame() override;
		virtual void SwapBuffers() override;
		virtual void Shutdown() override;

		void RecreateSwapchain();

		//Framebuffer Callback
		static void FramebufferResizeCallback(GLFWwindow* window, int width, int height);

		// Debug
		static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
		void* userData);

	private:
	// stay here!
		// Instance
		void CreateInstance();
		bool CheckValidationLayerSupport();
		std::vector<const char*> GetRequiredExtensions();
		void CheckRequiredExtensions(const char** extensions, uint32_t glfWExtensionsCount);

		// Debug
		void SetupDebugMessenger();
		void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

		// Surface
		void CreateSurface();		
		
		// Swapchain
		void CreateSwapchain();
		VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
		VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
		VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
		
		// Image Views
		VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspects);
		void CreateImageViews();

		// Renderpass
		void CreateRenderPass();

		// Graphics Pipeline
		void CreateDescriptorSetLayout();
		void CreateGraphicsPipeline();
		VkShaderModule CreateShaderModule(const std::vector<char>& code);
		std::vector<char> ReadFile(const std::string& filepath);

		// Framebuffers
		void CreateFramebuffers();
		
		// Commands
		void CreateCommandPool();

		void CreateDepthResources();
		VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
		VkFormat FindDepthFormat();
		bool HasStencilComponent(VkFormat format);

		void CreateTextureImage();
		void CreateTextureImageView();
		void CreateTextureSampler();
		void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
		void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

		void CreateVertexBuffer();
		void CreateIndexBuffer();
		void CreateUniformBuffers();

		void CreateDescriptorPool();
		void CreateDescriptorSets();

		void CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& memory);
		void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, 
			VkBuffer& buffer, VkDeviceMemory& memory, uint32_t familyIndexCount = 0, uint32_t* familyIndices = nullptr, VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE);
		void CopyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size);
		uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags propertiyFlags);

		VkCommandBuffer BeginSingleTimeCommands(VkCommandPool commandPool);
		void EndSingleTimeCommands(VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool commandPool);
		void CreateCommandBuffers();

		// Semaphores
		void CreateSyncObjects();

		
		void CleanupSwapchain();

		void UpdateUniformBuffer(uint32_t currentImage);

	private:
		GLFWwindow* m_WindowHandle;
		VkInstance m_Instance = VK_NULL_HANDLE;
		VkDebugUtilsMessengerEXT m_DebugMessenger;
		VkSurfaceKHR m_Surface;

		Ref<VulkanDevice> m_Device;

		VkSwapchainKHR m_Swapchain;
		std::vector<VkImage> m_SwapchainImages;
		std::vector<VkImageView> m_SwapchainImageViews;
		VkFormat m_SwapchainImageFormat;
		VkExtent2D m_SwapchainExtent;

		std::vector<VkFramebuffer> m_SwapchainFramebuffers;

		VkRenderPass m_RenderPass;

		VkDescriptorSetLayout m_DescriptorSetLayout;
		VkPipelineLayout m_PipelineLayout;
		VkPipeline m_GraphicsPipeline;

		VkCommandPool m_CommandPoolGraphics;
		VkCommandPool m_CommandPoolTransfer;

		VkImage m_TextureImage;
		VkImageView m_TextureImageView;
		VkDeviceMemory m_TextureImageMemory;
		VkSampler m_TextureSampler;

		VkImage m_DepthImage;
		VkDeviceMemory m_DepthImageMemory;
		VkImageView m_DepthImageView;

		VkBuffer m_VertexBuffer;
		VkDeviceMemory m_VertexBufferMemory;
		VkBuffer m_IndexBuffer;
		VkDeviceMemory m_IndexBufferMemory;
		std::vector<VkBuffer> m_UniformBuffers;
		std::vector<VkDeviceMemory> m_UniformBuffersMemory;

		VkDescriptorPool m_DescriptorPool;
		std::vector<VkDescriptorSet> m_DescriptorSets;

		std::vector<VkCommandBuffer> m_CommandBuffersGraphics;
		VkCommandBuffer m_CommandBufferTransfer;

		std::vector<VkSemaphore> m_ImageAvailableSemaphores;
		std::vector<VkSemaphore> m_RenderFinishedSemaphores;
		std::vector<VkFence> m_InFlightFence;
		std::vector<VkFence> m_ImagesInFlight;
		uint32_t m_CurrentFrame = 0;

		bool m_FramebufferResized = false;

		const std::vector<const char*> m_ValidationLayers = { "VK_LAYER_KHRONOS_validation" };
		
		const uint32_t MAX_FRAMES_IN_FLIGHT = 2;

		const std::vector<VertexData> m_Vertices = {
			{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
			{{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
			{{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
			{{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},

			{{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
			{{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
			{{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
			{{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}
		};

		const std::vector<uint16_t> m_Indices = {
			 0, 1, 2, 2, 3, 0,
			 4, 5, 6, 6, 7, 4
		};


//#ifdef PX_DEBUG
		const bool m_EnableValidationLayers = true;
//#else
	//	const bool m_EnableValidationLayers = false;
//#endif

	};
}
