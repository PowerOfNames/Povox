#include "pxpch.h"
#include "VulkanSwapchain.h"

#include "Povox/Core/Application.h"
#include "Povox/Renderer/Renderer.h"

#include "Platform/Vulkan/VulkanCommands.h"
#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanDebug.h"
#include "Platform/Vulkan/VulkanUtilities.h"


namespace Povox {

	VkSurfaceKHR VulkanSwapchain::s_Surface = VK_NULL_HANDLE;

	VulkanSwapchain::VulkanSwapchain(GLFWwindow* windowHandle)
	{
		PX_CORE_INFO("VulkanSwapchain::VulkanSwapchain: Creating...");

		PX_CORE_ASSERT(windowHandle, "Window handle is null");
		m_WindowHandle = windowHandle;


		//TODO: Potentially move somewhere else BUT window handle is needed...
		PX_CORE_VK_ASSERT(glfwCreateWindowSurface(VulkanContext::GetInstance(), m_WindowHandle, nullptr, &s_Surface), VK_SUCCESS, "Failed to create window surface!");
		PX_CORE_INFO("VulkanSurface created.");
		
		PX_CORE_INFO("VulkanSwapchain::VulkanSwapchain: Complete.");
	}

	void VulkanSwapchain::Recreate(uint32_t width, uint32_t height)
	{
		PX_PROFILE_FUNCTION();

		if (width <= 0 || height <= 0)
			return;

		PX_CORE_INFO("VulkanSwapchain::Recreate: Starting...");
		m_Properties.Width = width;
		m_Properties.Height = height;


		Ref<VulkanDevice> devicePtr = VulkanContext::GetDevice();
		VkDevice device = devicePtr->GetVulkanDevice();
		vkDeviceWaitIdle(device);

		Cleanup();

		uint32_t maxFramesInFlight = Application::Get()->GetSpecification().MaxFramesInFlight;

		SwapchainSupportDetails swapchainSupportDetails = devicePtr->QuerySwapchainSupport(devicePtr->GetPhysicalDevice());
		ChooseSwapSurfaceFormat(swapchainSupportDetails.Formats);
		ChooseSwapPresentMode(swapchainSupportDetails.PresentModes);
		ChooseSwapExtent(swapchainSupportDetails.Capabilities, width, height);


		uint32_t imageCount = maxFramesInFlight + 1; // +1 to avoid waiting for driver to finish internal operations
		if (swapchainSupportDetails.Capabilities.maxImageCount > 0 && imageCount > swapchainSupportDetails.Capabilities.maxImageCount)
			imageCount = swapchainSupportDetails.Capabilities.maxImageCount;
		PX_CORE_TRACE("Choosen SwapchainImageCount: '{0}'", imageCount);

		
		VkSurfaceCapabilitiesKHR caps{};
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(devicePtr->GetPhysicalDevice(), s_Surface, &caps);

		VkSwapchainCreateInfoKHR createInfo{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
		createInfo.pNext = nullptr;
		createInfo.flags = 0;
		
		createInfo.clipped = VK_TRUE;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.imageArrayLayers = 1;
		createInfo.imageColorSpace = m_Properties.SurfaceFormat.colorSpace;
		createInfo.imageExtent = { width, height };
		createInfo.imageFormat = m_Properties.SurfaceFormat.format;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		if (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
			createInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		if (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
			createInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		QueueFamilies queueFamIndices = devicePtr->GetQueueFamilies();
		uint32_t queueFamilyIndices[] = {
						queueFamIndices.GraphicsFamilyIndex,
						queueFamIndices.PresentFamilyIndex,
						queueFamIndices.TransferFamilyIndex
		};
		if (queueFamIndices.GraphicsFamilyIndex != queueFamIndices.PresentFamilyIndex)
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0;		// optional
			createInfo.pQueueFamilyIndices = nullptr;	// optional
		}
		createInfo.minImageCount = imageCount;
		createInfo.oldSwapchain = m_OldSwapchain;
		createInfo.presentMode = m_Properties.PresentMode;
		createInfo.preTransform = swapchainSupportDetails.Capabilities.currentTransform;
		createInfo.surface = s_Surface;
		PX_CORE_VK_ASSERT(vkCreateSwapchainKHR(device, &createInfo, nullptr, &m_Swapchain), VK_SUCCESS, "Failed to create logical device!");

		// Images		
		vkGetSwapchainImagesKHR(device, m_Swapchain, &imageCount, nullptr);
		m_Images.resize(imageCount);
		vkGetSwapchainImagesKHR(device, m_Swapchain, &imageCount, m_Images.data());
		m_Properties.ImageCount = imageCount;
		m_ImageViews.resize(m_Images.size());
		for (size_t i = 0; i < m_ImageViews.size(); i++)
		{
			VkImageViewCreateInfo info{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
			info.pNext = nullptr;
			info.flags = 0;

			info.image = m_Images[i];
			info.viewType = VK_IMAGE_VIEW_TYPE_2D;
			info.format = m_Properties.SurfaceFormat.format;

			info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

			info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			info.subresourceRange.baseMipLevel = 0;
			info.subresourceRange.levelCount = 1;
			info.subresourceRange.baseArrayLayer = 0;
			info.subresourceRange.layerCount = 1;
			PX_CORE_VK_ASSERT(vkCreateImageView(device, &info, nullptr, &m_ImageViews[i]), VK_SUCCESS, "Failed to create image view!");

#ifdef PX_DEBUG
			VkDebugUtilsObjectNameInfoEXT nameInfo{};
			nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
			nameInfo.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
			nameInfo.objectHandle = (uint64_t)m_ImageViews[i];
			std::string objDebugName = "Swapchain-ImageView" + std::to_string(i);
			nameInfo.pObjectName = objDebugName.c_str();
			NameVkObject(VulkanContext::GetDevice()->GetVulkanDevice(), nameInfo);
#endif // DEBUG
		}

		{
			m_DepthFormat = VulkanUtils::FindDepthFormat(devicePtr->GetPhysicalDevice());
			VkExtent3D extent{};
			extent.width = m_Properties.Width;
			extent.height = m_Properties.Height;
			extent.depth = 1;

			VkImageCreateInfo info{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
			info.pNext = nullptr;
			info.flags = 0;

			info.imageType = VK_IMAGE_TYPE_2D;
			info.extent = extent;
			info.format = m_DepthFormat;

			info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			info.mipLevels = 1;
			info.arrayLayers = 1;
			info.tiling = VK_IMAGE_TILING_OPTIMAL;
			info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;	// used as destination from the buffer to image, and the shader needs to sample from it
			info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;				// exclusive means it will only be used by one queue family (graphics and therefore also transfer possible)
			info.samples = VK_SAMPLE_COUNT_1_BIT;						// related to multi sampling -> in attachments
			info.queueFamilyIndexCount = 0;
			info.pQueueFamilyIndices = nullptr;

			VmaAllocationCreateInfo allocationInfo{};
			allocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
			PX_CORE_VK_ASSERT(vmaCreateImage(VulkanContext::GetAllocator(), &info, &allocationInfo, &m_DepthImage, &m_DepthImageAllocation, nullptr), VK_SUCCESS, "Failed to create Image!");

			VkImageViewCreateInfo viewInfo{};
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = m_DepthImage;
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = m_DepthFormat;

			viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			viewInfo.subresourceRange.baseMipLevel = 0;
			viewInfo.subresourceRange.levelCount = 1;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = 1;
			PX_CORE_VK_ASSERT(vkCreateImageView(device, &viewInfo, nullptr, &m_DepthImageView), VK_SUCCESS, "Failed to create ImageView!");

#ifdef PX_DEBUG
			VkDebugUtilsObjectNameInfoEXT nameInfo{};
			nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
			nameInfo.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
			nameInfo.objectHandle = (uint64_t)m_DepthImageView;
			nameInfo.pObjectName = "Swapchain-DepthImageView";
			NameVkObject(VulkanContext::GetDevice()->GetVulkanDevice(), nameInfo);
#endif // DEBUG

			VulkanCommandControl::ImmidiateSubmit(VulkanCommandControl::SubmitType::SUBMIT_TYPE_GRAPHICS_GRAPHICS, [=](VkCommandBuffer cmd)
				{
					VkImageMemoryBarrier barrier{};
					barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
					barrier.image = m_DepthImage;
					barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
					barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
					barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
					if (m_DepthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT || m_DepthFormat == VK_FORMAT_D24_UNORM_S8_UINT)
						barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
					barrier.subresourceRange.baseMipLevel = 0;
					barrier.subresourceRange.levelCount = 1;
					barrier.subresourceRange.baseArrayLayer = 0;
					barrier.subresourceRange.layerCount = 1;

					VkPipelineStageFlags srcStage;
					VkPipelineStageFlags dstStage;

					barrier.srcAccessMask = 0;
					barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

					srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
					dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

					vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
				});
		}

		// RenderPass
		if (!m_RenderPass)
		{
			VkAttachmentDescription colorAttachment{};
			colorAttachment.format = m_Properties.SurfaceFormat.format;
			colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
			colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

			VkAttachmentReference colorRef{};
			colorRef.attachment = 0;
			colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			VkAttachmentDescription depthAttachment{};
			depthAttachment.format = m_DepthFormat;
			depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
			depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			VkAttachmentReference depthRef{};
			depthRef.attachment = 1;
			depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			VkSubpassDescription subpass{};
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.colorAttachmentCount = 1;
			subpass.pColorAttachments = &colorRef;
			subpass.pDepthStencilAttachment = &depthRef;

			VkSubpassDependency dependency{};
			dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
			dependency.dstSubpass = 0;
			dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			dependency.srcAccessMask = 0;
			dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;


			std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
			VkRenderPassCreateInfo renderPassInfo{};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			renderPassInfo.pNext = nullptr;
			renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
			renderPassInfo.pAttachments = attachments.data();
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpass;
			renderPassInfo.dependencyCount = 1;
			renderPassInfo.pDependencies = &dependency;

			PX_CORE_VK_ASSERT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &m_RenderPass), VK_SUCCESS, "Failed to create renderpass!");
		}

		// Framebuffers
		m_Framebuffers.resize(m_ImageViews.size());
		VkFramebufferCreateInfo fbInfo{};
		fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fbInfo.pNext = nullptr;
		fbInfo.width = width;
		fbInfo.height = height;
		fbInfo.renderPass = m_RenderPass;
		fbInfo.layers = 1;
		for (size_t i = 0; i < m_ImageViews.size(); i++)
		{
			std::array<VkImageView, 2> imageViews = { m_ImageViews[i], m_DepthImageView };
			fbInfo.attachmentCount = static_cast<uint32_t>(imageViews.size());
			fbInfo.pAttachments = imageViews.data();
			PX_CORE_VK_ASSERT(vkCreateFramebuffer(device, &fbInfo, nullptr, &m_Framebuffers[i]), VK_SUCCESS, "Failed to create framebuffer!");
		}

		if(Application::Get()->GetSpecification().State.RendererInitialized)
			Renderer::OnSwapchainRecreate();

		PX_CORE_INFO("Swapchain (re)creation with: '({0}|{1})', '{2}' Images and '{3}' FramesInFlight!", width, height, m_Images.size(), maxFramesInFlight);
		PX_CORE_INFO("VulkanSwapchain::Recreate: Completed.");
	}

	void VulkanSwapchain::Cleanup()
	{
		VkDevice device = VulkanContext::GetDevice()->GetVulkanDevice();
		vkDeviceWaitIdle(device);

		vmaDestroyImage(VulkanContext::GetAllocator(), m_DepthImage, m_DepthImageAllocation);
		vkDestroyImageView(device, m_DepthImageView, nullptr);

		for (size_t i = 0; i < m_Framebuffers.size(); i++)
		{
			vkDestroyFramebuffer(device, m_Framebuffers[i], nullptr);
		}
		m_Framebuffers.clear();

		if (m_RenderPass)
		{
			vkDestroyRenderPass(device, m_RenderPass, nullptr);
			m_RenderPass = VK_NULL_HANDLE;
		}

		for (auto& imageView : m_ImageViews)
		{
			vkDestroyImageView(device, imageView, nullptr);
		}
		m_ImageViews.clear();
		m_Images.clear();

		if (m_OldSwapchain != VK_NULL_HANDLE)
		{
			vkDestroySwapchainKHR(device, m_OldSwapchain, nullptr);
		}
		m_OldSwapchain = m_Swapchain;
		m_Swapchain = VK_NULL_HANDLE;
	}

	void VulkanSwapchain::Destroy()
	{
		PX_PROFILE_FUNCTION();

		VkDevice device = VulkanContext::GetDevice()->GetVulkanDevice();
		vkDeviceWaitIdle(device);

		Cleanup();
		if(m_OldSwapchain)
			vkDestroySwapchainKHR(device, m_OldSwapchain, nullptr);
		m_OldSwapchain = VK_NULL_HANDLE;

		vkDestroySurfaceKHR(VulkanContext::GetInstance(), s_Surface, nullptr);
		s_Surface = VK_NULL_HANDLE;
	}

	SwapchainFrame* VulkanSwapchain::AcquireNextImageIndex(VkSemaphore presentSemaphore)
	{
		PX_PROFILE_FUNCTION();


		m_CurrentFrame.LastImageIndex = m_CurrentFrame.CurrentImageIndex;
		VkResult result = vkAcquireNextImageKHR(VulkanContext::GetDevice()->GetVulkanDevice(), m_Swapchain, UINT64_MAX /*disables timeout*/, presentSemaphore, VK_NULL_HANDLE, &m_CurrentFrame.CurrentImageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			PX_CORE_WARN("VulkanSwapchain::AcquireNextImageIndex Out Of Date -> Recreation!");
			Recreate(Application::Get()->GetWindow().GetWidth(), Application::Get()->GetWindow().GetHeight()); // get the new size maybe from elsewhere -> might be that the size change caused the result!
			//TODO: investigate if this works!
			return nullptr;
		}
		else
		{
			PX_CORE_ASSERT(!((result != VK_SUCCESS) && (result != VK_SUBOPTIMAL_KHR)), "Failed to acquire swap chain image!");
		}

		m_CurrentFrame.CurrentImage = m_Images[m_CurrentFrame.CurrentImageIndex];
		m_CurrentFrame.CurrentImageView = m_ImageViews[m_CurrentFrame.CurrentImageIndex];
		m_CurrentFrame.Commands.clear();

		return &m_CurrentFrame;
	}

	void VulkanSwapchain::SwapBuffers()
	{
		PX_PROFILE_FUNCTION();


		QueueSubmit();
		Present();
	}	

	void VulkanSwapchain::QueueSubmit()
	{
		PX_PROFILE_FUNCTION();


		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submitInfo.pNext = nullptr;
		
		submitInfo.commandBufferCount = static_cast<uint32_t>(m_CurrentFrame.Commands.size());
		submitInfo.pCommandBuffers = m_CurrentFrame.Commands.data();

		submitInfo.pWaitDstStageMask = waitStages;		
		submitInfo.waitSemaphoreCount = static_cast<uint32_t>(m_CurrentFrame.WaitSemaphores.size());
		submitInfo.pWaitSemaphores = m_CurrentFrame.WaitSemaphores.data();

		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &m_CurrentFrame.RenderSemaphore;

		PX_CORE_VK_ASSERT(vkQueueSubmit(VulkanContext::GetDevice()->GetQueueFamilies().Queues.GraphicsQueue, 1, &submitInfo, m_CurrentFrame.CurrentFence), VK_SUCCESS, "Failed to submit draw render buffer!");
	}

	void VulkanSwapchain::Present()
	{
		PX_PROFILE_FUNCTION();


		VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
		presentInfo.pNext = nullptr;

		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &m_Swapchain;
		presentInfo.pWaitSemaphores = &m_CurrentFrame.RenderSemaphore;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pImageIndices = &m_CurrentFrame.CurrentImageIndex;
		presentInfo.pResults = nullptr;

		VkResult result;
		result = vkQueuePresentKHR(VulkanContext::GetDevice()->GetQueueFamilies().Queues.PresentQueue, &presentInfo);

		//check if the framebuffer resized
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_FramebufferResized)
		{
			m_FramebufferResized = false;
			Recreate(Application::Get()->GetWindow().GetWidth(), Application::Get()->GetWindow().GetHeight());
		}
		else
		{
			PX_CORE_ASSERT(result == VK_SUCCESS, "Failed to present swap chain image!");
		}
	}


	void VulkanSwapchain::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
	{
		VkSurfaceFormatKHR result;
		for (const auto& format : availableFormats)
		{
			if (format.format == VK_FORMAT_R8G8B8A8_SRGB && format.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR)
			{
				result = format;
				PX_CORE_INFO("Swapchain surface format:  {0}", result.format);
				m_Properties.SurfaceFormat = format;
			}
		}
		result = availableFormats[0];
		PX_CORE_INFO("Swapchain surface format:  {0}", result.format);
		m_Properties.SurfaceFormat = result;
	}
	void VulkanSwapchain::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
	{
		for (const auto& mode : availablePresentModes)
		{
			if (mode == VK_PRESENT_MODE_MAILBOX_KHR) // on mobile, FIFO_KHR is more likely to be used because of lower enegry consumption
				m_Properties.PresentMode = mode;
		}
		m_Properties.PresentMode = VK_PRESENT_MODE_FIFO_KHR;
	}
	void VulkanSwapchain::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height)
	{
		if (capabilities.currentExtent.width != UINT32_MAX)
		{
			m_Properties.Width = capabilities.currentExtent.width;
			m_Properties.Height = capabilities.currentExtent.height;
		}

		else
		{
			VkExtent2D actualExtend = { width, height };

			actualExtend.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtend.width));
			actualExtend.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtend.height));

			m_Properties.Width = actualExtend.width;
			m_Properties.Height = actualExtend.height;
		}
		PX_CORE_INFO("Swapchain extent: '[{0}|{1}]'", m_Properties.Width, m_Properties.Height);
	}
}
