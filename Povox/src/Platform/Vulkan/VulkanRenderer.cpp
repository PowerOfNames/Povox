#include "pxpch.h"
#include "Platform/Vulkan/VulkanRenderer.h"

#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanDebug.h"
#include "Platform/Vulkan/VulkanFramebuffer.h"
#include "Platform/Vulkan/VulkanQueryManager.h"

#include "Povox/Systems/TextureSystem.h"

#include <vulkan/vulkan.h>


#include <glm/glm.hpp>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>

namespace Povox {

	Scope<VulkanImGui> VulkanRenderer::m_ImGui = nullptr;
	static constexpr uint32_t MAX_OBJECTS = 10000;

	VulkanRenderer::VulkanRenderer(const RendererSpecification& specs)
		:m_Specification(specs)
	{
	}

	VulkanRenderer::~VulkanRenderer()
	{
		PX_CORE_TRACE("Vulkan Renderer API delete!");
	}

	// Core
	bool VulkanRenderer::Init()
	{
		PX_PROFILE_FUNCTION();


		PX_CORE_INFO("VulkanRenderer::Init: Started initializing...");

		m_Device = VulkanContext::GetDevice()->GetVulkanDevice();
		PX_CORE_ASSERT(m_Device, "No VulkanDevice was set!");
		m_Swapchain = Application::Get()->GetWindow().GetSwapchain();
		PX_CORE_ASSERT(m_Device, "No VulkanSwapchain was set!");

		InitCommandControl();		
		InitFrameData();


		PX_CORE_INFO("Creating QueryManager...");

		InitPerformanceQueryPools();

		PX_CORE_INFO("Completed QueryManager creation.");


		
		PX_CORE_INFO("Creating ShaderLibrary...");

		m_ShaderManager = CreateRef<ShaderManager>(Application::Get()->GetSpecification().ShaderFilePath);

		PX_CORE_INFO("Completed ShaderLibrary creation.");


		PX_CORE_INFO("Creating TextureSystem...");

		m_TextureSystem = CreateRef<TextureSystem>();

		PX_CORE_INFO("Completed TextureSystem creation.");

		PX_CORE_INFO("Creating MaterialSystem...");

		m_MaterialSystem = CreateRef<VulkanMaterialSystem>();

		PX_CORE_INFO("Completed MaterialSystem creation.");

		PX_CORE_INFO("Creating ShaderResourceSystem...");

		m_ShaderResourceSystem = CreateRef<VulkanShaderResourceSystem>();

		PX_CORE_INFO("Completed ShaderResourceSystem creation.");


		CreateSamplers();
		CreateDescriptors();

		m_ImGui = CreateScope<VulkanImGui>(Application::Get()->GetWindow().GetSwapchain()->GetImageFormat(), (uint8_t)m_Specification.MaxFramesInFlight);
		PX_CORE_TRACE("VulkanRenderer:: Created VKImGui!");
		

		

		m_Statistics.State = &m_Specification.State;
		m_Specification.State.IsInitialized = true;
		PX_CORE_INFO("VulkanRenderer::Init: Completed initialization.");
		return m_Specification.State.IsInitialized;
	}

	void VulkanRenderer::Shutdown()
	{
		PX_PROFILE_FUNCTION();


		PX_CORE_INFO("VulkanRenderer::Shutdown: Starting...");

		vkDeviceWaitIdle(m_Device);

		PX_CORE_INFO("Starting ShaderLibrary shutdown...");

		m_ShaderManager->Shutdown();

		PX_CORE_INFO("Completed ShaderLibrary shutdown.");

		PX_CORE_INFO("Starting TextureSystem shutdown...");

		m_TextureSystem->Shutdown();

		PX_CORE_INFO("Completed TextureSystem shutdown.");

		PX_CORE_INFO("Started destruction of performance pools...");


		m_QueryManager->Shutdown();

		PX_CORE_INFO("Started destruction of performance pools...");
		PX_CORE_INFO("Started destruction of FrameObjects (Sync, Commands, UBOs) for {0} frames...", m_Frames.size());

		for (size_t i = 0; i < m_Frames.size(); i++)
		{
			vkDestroySemaphore(m_Device, m_Frames[i].Semaphores.PresentSemaphore, nullptr);
			vkDestroySemaphore(m_Device, m_Frames[i].Semaphores.RenderSemaphore, nullptr);

			vkDestroySemaphore(m_Device, m_Frames[i].Semaphores.ComputeFinishedSemaphore, nullptr);

			vkDestroyFence(m_Device, m_Frames[i].RenderFence, nullptr);
			vkDestroyFence(m_Device, m_Frames[i].ComputeFence, nullptr);

			vkDestroyCommandPool(m_Device, m_Frames[i].Commands.Pool, nullptr);

			vmaDestroyBuffer(VulkanContext::GetAllocator(), m_Frames[i].CamUniformBuffer.Buffer, m_Frames[i].CamUniformBuffer.Allocation);
			vmaDestroyBuffer(VulkanContext::GetAllocator(), m_Frames[i].ObjectBuffer.Buffer, m_Frames[i].ObjectBuffer.Allocation);
		}
		m_Frames.clear();

		PX_CORE_INFO("Completed FrameObjects (Synch, Commands, UBOs) destruction for {0} frames...", m_Frames.size());
		PX_CORE_WARN("Started destruction of leftovers and other things...");

		vmaDestroyBuffer(VulkanContext::GetAllocator(), m_SceneParameterBuffer.Buffer, m_SceneParameterBuffer.Allocation);

		m_CommandControl->Destroy();

		m_ImGui->Destroy();

		for(uint32_t i = 0; i < m_FinalImages.size(); i++)
			m_FinalImages[i]->Free();
		m_FinalImages.clear();


		vkDestroySampler(m_Device, m_TextureSampler, nullptr);

		

		PX_CORE_WARN("Completed leftover and other things' destruction.");
		PX_CORE_INFO("VulkanRenderer::Shutdown: Completed.");
	}
	
	void VulkanRenderer::WaitForDeviceFinished()
	{
		//PX_CORE_VK_ASSERT(vkDeviceWaitIdle(m_Device), VK_SUCCESS, "Failed to wait for device");
	}

	//FrameData
	bool VulkanRenderer::PrepareRenderFrame()
	{
		vkWaitForFences(m_Device, 1, &GetCurrentFrame().RenderFence, VK_TRUE, UINT64_MAX);
		vkResetCommandPool(m_Device, GetCurrentFrame().Commands.Pool, 0);
		
		m_SwapchainFrame = m_Swapchain->AcquireNextImageIndex(GetCurrentFrame().Semaphores.PresentSemaphore);
		if (!m_SwapchainFrame)
			return false;
		vkResetFences(m_Device, 1, &GetCurrentFrame().RenderFence);

		m_Specification.State.CurrentSwapchainImageIndex = m_CurrentSwapchainImageIndex = m_SwapchainFrame->CurrentImageIndex;
		m_SwapchainFrame->CurrentFence = GetCurrentFrame().RenderFence;
		m_SwapchainFrame->WaitSemaphores.clear();
		m_SwapchainFrame->WaitSemaphores.push_back(GetCurrentFrame().Semaphores.PresentSemaphore);
		m_SwapchainFrame->RenderSemaphore = GetCurrentFrame().Semaphores.RenderSemaphore;

		return true;
	}

	bool VulkanRenderer::PrepareComputeFrame()
	{
		vkWaitForFences(m_Device, 1, &GetCurrentFrame().ComputeFence, VK_TRUE, UINT64_MAX);		
		vkResetCommandPool(m_Device, GetCurrentFrame().Commands.ComputePool, 0);
		
		vkResetFences(m_Device, 1, &GetCurrentFrame().ComputeFence);
		//vkResetCommandBuffer(GetCurrentFrame().Commands.ComputeBuffer, 0);

		//m_SwapchainFrame->WaitSemaphores.push_back(GetCurrentFrame().Semaphores.ComputeFinishedSemaphore);

		return true;
	}

	/**
	 * Clears resources form last frames garbage collection.
	 * Queries performance results form last Frame.
	 * Prepares preProcess ComputeResources.
	 */
	bool VulkanRenderer::BeginFrame()
	{
		PX_PROFILE_FUNCTION();

		m_QueryManager->ResetTimestampQueryPool(m_CurrentFrameIndex);
		m_QueryManager->ResetPipelineQueryPools(m_CurrentFrameIndex);
		//VulkanContext::FreeFrameResources(m_CurrentFrameIndex);

		return PrepareRenderFrame() && PrepareComputeFrame();
	}
	void VulkanRenderer::EndFrame()
	{
		GetQueryResults(m_CurrentFrameIndex);
		m_Specification.State.LastFrameIndex = m_LastFrameIndex = m_CurrentFrameIndex;
		m_Specification.State.CurrentFrameIndex = m_CurrentFrameIndex = (++m_CurrentFrameIndex) % m_Specification.MaxFramesInFlight;
		m_Specification.State.TotalFrames++;
	}
	
	void VulkanRenderer::Draw(Ref<Buffer> vertices, uint64_t firstVertexOffset, Ref<Material> material, Ref<Buffer> indices, size_t indexCount, bool textureless)
	{
		PX_PROFILE_FUNCTION();

		if(material)
			Ref<VulkanShader> vkShader = std::dynamic_pointer_cast<VulkanShader>(material->GetShader());

		Ref<VulkanBuffer> verticesVkBuf = std::dynamic_pointer_cast<VulkanBuffer>(vertices);
		verticesVkBuf->TransferOwnership(QueueFamilyOwnership::QFO_GRAPHICS, firstVertexOffset, indexCount * verticesVkBuf->GetSpecification().ElementSize);


		uint32_t frameIndex = m_CurrentFrameIndex % m_Specification.MaxFramesInFlight;
		

		/* Instead of using memcpy here, we are doing a different trick. It is possible to cast the void* from mapping the buffer into another type, and write into it normally.
		* This will work completely fine, and makes it easier to write complex types into a buffer.
		**/
		//mapping of model data into the SSBO object buffer
// 		uint32_t objectUniformOffset = PadUniformBuffer(sizeof(ObjectUniform), VulkanContext::GetDevice()->GetPhysicalDeviceProperties().limits.minUniformBufferOffsetAlignment) * (size_t)frameIndex;
// 
// 		ObjectUniform objectUniform;
// 		//objectUniform.ModelMatrix = renderable.ModelMatrix;
// 		objectUniform.TexID = 0;
// 		objectUniform.TilingFactor = 1.0f;
// 
// 		void* data;
// 		vmaMapMemory(VulkanContext::GetAllocator(), GetCurrentFrame().ObjectBuffer.Allocation, &data);
// 		memcpy(data, &objectUniform, sizeof(ObjectUniform));
// 		vmaUnmapMemory(VulkanContext::GetAllocator(), GetCurrentFrame().ObjectBuffer.Allocation);

// 		vkCmdBindDescriptorSets(
// 			m_ActiveCommandBuffer, 
// 			VK_PIPELINE_BIND_POINT_GRAPHICS, 
// 			m_ActivePipeline->GetLayout(), 
// 			1, 
// 			1, 
// 			&GetCurrentFrame().ObjectDescriptorSet, 
// 			0, 
// 			&objectUniformOffset);
		

		// only bind material descriptor sets here -> better store them all in one descriptor set and pass the offsets
		
		if (!textureless)
		{
			VkWriteDescriptorSet samplerWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			samplerWrite.dstBinding = 0;
			samplerWrite.dstArrayElement = 0;
			samplerWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
			samplerWrite.descriptorCount = 1;
			samplerWrite.dstSet = GetCurrentFrame().TextureDescriptorSet;

			VkDescriptorImageInfo samplerInfo{};
			samplerInfo.sampler = m_TextureSampler;
			samplerWrite.pImageInfo = &samplerInfo;

			auto& activeTextures = m_TextureSystem->GetActiveTextures();
			//TODO: Get maxImageSlots from config as const
			std::array<VkDescriptorImageInfo, 32> imageInfos;
			for (uint32_t i = 0; i < activeTextures.size(); i++)
			{
				imageInfos[i] = std::dynamic_pointer_cast<VulkanImage2D>(activeTextures[i]->GetImage())->GetImageInfo();
			}
			VkWriteDescriptorSet setWrites{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			setWrites.dstBinding = 1;
			setWrites.dstArrayElement = 0;
			setWrites.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			setWrites.descriptorCount = imageInfos.size();
			setWrites.pBufferInfo = 0;
			setWrites.dstSet = GetCurrentFrame().TextureDescriptorSet;
			setWrites.pImageInfo = imageInfos.data();


			VkWriteDescriptorSet writes[2] = { samplerWrite, setWrites };

			vkUpdateDescriptorSets(m_Device, 2, writes, 0, nullptr);

			vkCmdBindDescriptorSets(
				m_ActiveCommandBuffer,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				m_ActivePipeline->GetLayout(),
				2,
				1,
				&GetCurrentFrame().TextureDescriptorSet,
				0,
				nullptr
			);
		}
		

		VkBuffer vertexBuffer = verticesVkBuf->GetAllocation().Buffer;
		VkDeviceSize offsets[] = { firstVertexOffset };
		vkCmdBindVertexBuffers(m_ActiveCommandBuffer, 0, 1, &vertexBuffer, offsets);
		if (indices)
		{
			VkBuffer indexBuffer = std::dynamic_pointer_cast<VulkanBuffer>(indices)->GetAllocation().Buffer;
			vkCmdBindIndexBuffer(m_ActiveCommandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(m_ActiveCommandBuffer, indexCount, 1, 0, 0, 0);
		}
		else
		{
			//hardcoded -> indexCount as vertex count for particleRendering -> points. I set the indexCount in the frontend to be the particleCount, as indices are not used anyways
			vkCmdDraw(m_ActiveCommandBuffer, indexCount, 1, 0, 0);
		}

		if(m_QueryActive)
			//m_QueryManager->EndPipelineQuery("PipelineQueryPool", m_ActiveCommandBuffer, m_CurrentFrameIndex);		
		
		verticesVkBuf->TransferOwnership(QueueFamilyOwnership::QFO_COMPUTE, firstVertexOffset, indexCount * verticesVkBuf->GetSpecification().ElementSize);
	}

	void VulkanRenderer::DrawRenderable(const Renderable& renderable)
	{
		PX_PROFILE_FUNCTION();


		Ref<VulkanShader> vkShader = std::dynamic_pointer_cast<VulkanShader>(renderable.Material.Shader);


		uint32_t frameIndex = m_CurrentFrameIndex % m_Specification.MaxFramesInFlight;
		uint32_t camUniformOffset = PadUniformBuffer(sizeof(SceneUniform), VulkanContext::GetDevice()->GetPhysicalDeviceProperties().limits.minUniformBufferOffsetAlignment) * (size_t)frameIndex;

		uint32_t height = m_Swapchain->GetProperties().Height;
		uint32_t width = m_Swapchain->GetProperties().Width;
		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = static_cast<float>(height);
		viewport.width = static_cast<float>(width);
		viewport.height = -static_cast<float>(height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(m_ActiveCommandBuffer, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = { width, height };
		vkCmdSetScissor(m_ActiveCommandBuffer, 0, 1, &scissor);


		vkCmdBindDescriptorSets(m_ActiveCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ActivePipeline->GetLayout(), 0, 1, &GetCurrentFrame().GlobalDescriptorSet, 1, &camUniformOffset);
				
		/* Instead of using memcpy here, we are doing a different trick. It is possible to cast the void* from mapping the buffer into another type, and write into it normally.
		* This will work completely fine, and makes it easier to write complex types into a buffer.
		**/
		//mapping of model data into the SSBO object buffer
		uint32_t objectUniformOffset = PadUniformBuffer(sizeof(ObjectUniform), VulkanContext::GetDevice()->GetPhysicalDeviceProperties().limits.minUniformBufferOffsetAlignment) * (size_t)frameIndex;
		
		ObjectUniform objectUniform;
		//objectUniform.ModelMatrix = renderable.ModelMatrix;
		objectUniform.TexID = renderable.Material.TexID;
		objectUniform.TilingFactor = renderable.Material.TilingFactor;

		void* data;
		vmaMapMemory(VulkanContext::GetAllocator(), GetCurrentFrame().ObjectBuffer.Allocation, &data);
		memcpy(data, &objectUniform, sizeof(ObjectUniform));
		vmaUnmapMemory(VulkanContext::GetAllocator(), GetCurrentFrame().ObjectBuffer.Allocation);
		
		vkCmdBindDescriptorSets(m_ActiveCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ActivePipeline->GetLayout(), 1, 1, &GetCurrentFrame().ObjectDescriptorSet, 0, &objectUniformOffset);

		if (renderable.Material.Texture)
		{		
			auto& activeTextures = m_TextureSystem->GetActiveTextures(); 
			//TODO: Get maxImageSlots form config as const
			std::array<VkDescriptorImageInfo, 32> imageInfos;
			for (uint32_t i = 0; i < activeTextures.size(); i++)
			{
				imageInfos[i] = std::dynamic_pointer_cast<VulkanImage2D>(activeTextures[i]->GetImage())->GetImageInfo(); 
			}
			VkWriteDescriptorSet setWrites{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			setWrites.dstBinding = 1;
			setWrites.dstArrayElement = 0;
			setWrites.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			setWrites.descriptorCount = imageInfos.size();
			setWrites.pBufferInfo = 0;
			setWrites.dstSet = GetCurrentFrame().TextureDescriptorSet;
			setWrites.pImageInfo = imageInfos.data();

			vkUpdateDescriptorSets(m_Device, 1, &setWrites, 0, nullptr);
			vkCmdBindDescriptorSets(m_ActiveCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ActivePipeline->GetLayout(), 2, 1, &GetCurrentFrame().TextureDescriptorSet, 0, nullptr);
		}
		
		VkBuffer vertexBuffer = std::dynamic_pointer_cast<VulkanBuffer>(renderable.MeshData.VertexBuffer)->GetAllocation().Buffer;
		VkBuffer indexBuffer = std::dynamic_pointer_cast<VulkanBuffer>(renderable.MeshData.IndexBuffer)->GetAllocation().Buffer;
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(m_ActiveCommandBuffer, 0, 1, &vertexBuffer, offsets);
		vkCmdBindIndexBuffer(m_ActiveCommandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);


		static auto startTime = std::chrono::high_resolution_clock::now();
		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
		float x = 0.0f + time;
		//glm::mat4 model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(x, 0.0f, 1.0f));
		//PushConstants pushConstant;
		//pushConstant.ModelMatrix = model;
		//vkCmdPushConstants(cmd, m_GraphicsPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pushConstant);
			
		vkCmdDrawIndexed(m_ActiveCommandBuffer, 6, 1, 0, 0, 0);
	}

	void VulkanRenderer::InitFrameData()
	{
		PX_PROFILE_FUNCTION();


		PX_CORE_INFO("VulkanRenderer::InitFrameData: Starting initialization...");

		uint32_t maxFrames = m_Specification.MaxFramesInFlight;
		PX_CORE_INFO("MaxFramesInFlight: {0}", maxFrames);

		if (m_Frames.size() != maxFrames)
		{
			m_Frames.resize(maxFrames);
			PX_CORE_INFO("Creating synchronization objects for {0} frames...", maxFrames);
			
			//Semaphores
			VkSemaphoreCreateInfo createSemaphoreInfo{};
			createSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
			for (size_t i = 0; i < maxFrames; i++)
			{
				PX_CORE_VK_ASSERT(vkCreateSemaphore(m_Device, &createSemaphoreInfo, nullptr, &m_Frames[i].Semaphores.RenderSemaphore), VK_SUCCESS, "Failed to create RenderSemaphore!");
				PX_CORE_VK_ASSERT(vkCreateSemaphore(m_Device, &createSemaphoreInfo, nullptr, &m_Frames[i].Semaphores.PresentSemaphore), VK_SUCCESS, "Failed to create PresentSemaphore!");
				
				PX_CORE_VK_ASSERT(vkCreateSemaphore(m_Device, &createSemaphoreInfo, nullptr, &m_Frames[i].Semaphores.ComputeFinishedSemaphore), VK_SUCCESS, "Failed to create ComputeFinishedSemaphore!");
			}
			//Fences
			VkFenceCreateInfo createFenceInfo{};
			createFenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			createFenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
			for (size_t i = 0; i < maxFrames; i++)
			{
				PX_CORE_VK_ASSERT(vkCreateFence(m_Device, &createFenceInfo, nullptr, &m_Frames[i].RenderFence), VK_SUCCESS, "Failed to create Render Fence!");

				PX_CORE_VK_ASSERT(vkCreateFence(m_Device, &createFenceInfo, nullptr, &m_Frames[i].ComputeFence), VK_SUCCESS, "Failed to create Compute Fence!");
			}

			PX_CORE_INFO("Completed Synchronization objects creation for {0} frames.", maxFrames);
			PX_CORE_INFO("Creating Command objects for {0} frames...", maxFrames);

			//Commands
			VkCommandPoolCreateInfo poolci{};
			poolci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			poolci.pNext = nullptr;
			poolci.queueFamilyIndex = VulkanContext::GetDevice()->GetQueueFamilies().GraphicsFamilyIndex;
			poolci.flags = 0;

			VkCommandBufferAllocateInfo bufferci{};
			bufferci.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			bufferci.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			bufferci.commandBufferCount = 1;

			for (uint32_t i = 0; i < maxFrames; i++)
			{
				PX_CORE_VK_ASSERT(vkCreateCommandPool(m_Device, &poolci, nullptr, &m_Frames[i].Commands.Pool), VK_SUCCESS, "Failed to create graphics command pool!");

#ifdef PX_DEBUG
				VkDebugUtilsObjectNameInfoEXT nameInfo{};
				nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
				nameInfo.objectType = VK_OBJECT_TYPE_COMMAND_POOL;
				nameInfo.objectHandle = (uint64_t)m_Frames[i].Commands.Pool;
				std::string debugName = "FrameGraphicsPool_Frame" + std::to_string(i);
				nameInfo.pObjectName = debugName.c_str();
				NameVkObject(VulkanContext::GetDevice()->GetVulkanDevice(), nameInfo);
#endif // DEBUG
				
				bufferci.commandPool = m_Frames[i].Commands.Pool;
				PX_CORE_VK_ASSERT(vkAllocateCommandBuffers(m_Device, &bufferci, &m_Frames[i].Commands.RenderBuffer), VK_SUCCESS, "Failed to create Render CommandBuffer!");

			}
			poolci.queueFamilyIndex = VulkanContext::GetDevice()->GetQueueFamilies().ComputeFamilyIndex;
			for (uint32_t i = 0; i < maxFrames; i++)
			{
				PX_CORE_VK_ASSERT(vkCreateCommandPool(m_Device, &poolci, nullptr, &m_Frames[i].Commands.ComputePool), VK_SUCCESS, "Failed to create graphics command pool!");

#ifdef PX_DEBUG
				VkDebugUtilsObjectNameInfoEXT nameInfo{};
				nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
				nameInfo.objectType = VK_OBJECT_TYPE_COMMAND_POOL;
				nameInfo.objectHandle = (uint64_t)m_Frames[i].Commands.ComputePool;
				std::string debugName = "FrameComputePool_Frame" + std::to_string(i);
				nameInfo.pObjectName = debugName.c_str();
				NameVkObject(VulkanContext::GetDevice()->GetVulkanDevice(), nameInfo);
#endif // DEBUG

				bufferci.commandPool = m_Frames[i].Commands.ComputePool;
				PX_CORE_VK_ASSERT(vkAllocateCommandBuffers(m_Device, &bufferci, &m_Frames[i].Commands.ComputeBuffer), VK_SUCCESS, "Failed to create Compute CommandBuffer!");
			}

			PX_CORE_INFO("Completed Command objects creation for {0} frames.", maxFrames);
			
			PX_CORE_INFO("Creating (global) UBOs for {0} frames...", maxFrames);

			const size_t align = VulkanContext::GetDevice()->GetPhysicalDeviceProperties().limits.minUniformBufferOffsetAlignment;
			const size_t sceneUniformSize = maxFrames * PadUniformBuffer(sizeof(SceneUniform), align);

			m_SceneParameterBuffer = VulkanBuffer::CreateAllocation(sceneUniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

			for (uint32_t i = 0; i < maxFrames; i++)
			{
				m_Frames[i].CamUniformBuffer = VulkanBuffer::CreateAllocation(sizeof(CameraUniform), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
				
				//TODO: Track these buffers sizes
				m_Frames[i].ObjectBuffer = VulkanBuffer::CreateAllocation(sizeof(ObjectUniform) * MAX_OBJECTS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
			}
			
			PX_CORE_INFO("Completed (global) UBO creation for {0} frames.", maxFrames);

			m_FramebufferWidth = m_ViewportWidth = m_Swapchain->GetProperties().Width;
			m_FramebufferHeight = m_ViewportHeight = m_Swapchain->GetProperties().Height;
			m_FinalImages.resize(maxFrames);
		}
		else
		{
			PX_CORE_WARN("VulkanRenderer::Init call after first initialization!");
		}


		PX_CORE_INFO("VulkanRenderer::InitFrameData: Completed initialization.");
	}

	void VulkanRenderer::CreateFinalImage(Ref<Image2D> sourceImage)
	{
		PX_PROFILE_FUNCTION();


		Ref<VulkanImage2D> sourceImageVK = std::dynamic_pointer_cast<VulkanImage2D>(sourceImage);
		if (!m_FinalImages[m_CurrentFrameIndex] 
			|| m_FinalImages[m_CurrentFrameIndex]->GetSpecification().Width != m_ViewportWidth 
			|| m_FinalImages[m_CurrentFrameIndex]->GetSpecification().Height != m_ViewportHeight)
		{
			InitFinalImage(m_ViewportWidth, m_ViewportHeight);
		}

		//Transition final image
		m_FinalImages[m_CurrentFrameIndex]->TransitionImageLayout(
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_PIPELINE_STAGE_2_NONE, 0,
			VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT | VK_ACCESS_2_TRANSFER_READ_BIT
		);

		//Transition source image
		sourceImageVK->TransitionImageLayout(
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
			VK_PIPELINE_STAGE_2_TRANSFER_BIT,	VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT
		);
		
		//Image copying
		m_CommandControl->ImmidiateSubmit(VulkanCommandControl::SubmitType::SUBMIT_TYPE_TRANSFER_TRANSFER, [=](VkCommandBuffer cmd)
			{
				VkImageCopy imageCopyRegion{};
				imageCopyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				imageCopyRegion.srcSubresource.layerCount = 1;
				imageCopyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				imageCopyRegion.dstSubresource.layerCount = 1;
				imageCopyRegion.extent.width = m_ViewportWidth;
				imageCopyRegion.extent.height = m_ViewportHeight;
				imageCopyRegion.extent.depth = 1;

				vkCmdCopyImage(cmd, sourceImageVK->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_FinalImages[m_CurrentFrameIndex]->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopyRegion);
			});
		// Transition framebuffer image back to graphics queue
		sourceImageVK->TransitionImageLayout(
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
		//Transition swapchain image back into color
		m_FinalImages[m_CurrentFrameIndex]->TransitionImageLayout(
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
			VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
	}
	void VulkanRenderer::PrepareSwapchainImage(Ref<Image2D> sourceImage)
	{
		PX_PROFILE_FUNCTION();


		Ref<VulkanImage2D> sourceImageVK = std::dynamic_pointer_cast<VulkanImage2D>(sourceImage);

		//Transition swapchain image
		m_CommandControl->ImmidiateSubmit(VulkanCommandControl::SubmitType::SUBMIT_TYPE_GRAPHICS_GRAPHICS, [=](VkCommandBuffer cmd) {
			VkImageMemoryBarrier barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.image = m_SwapchainFrame->CurrentImage;
			barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.levelCount = 1;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = 1;

			VkPipelineStageFlags srcStage;
			VkPipelineStageFlags dstStage;
			barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

			vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
			});
		//Transition final rendered image
		sourceImageVK->TransitionImageLayout(
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_PIPELINE_STAGE_2_TRANSFER_BIT,	VK_ACCESS_2_MEMORY_READ_BIT,
			VK_PIPELINE_STAGE_2_TRANSFER_BIT,	VK_ACCESS_2_TRANSFER_READ_BIT
		);
		//Image copying
		m_CommandControl->ImmidiateSubmit(VulkanCommandControl::SubmitType::SUBMIT_TYPE_TRANSFER_TRANSFER, [=](VkCommandBuffer cmd)
			{
				VkImageCopy imageCopyRegion{};
				imageCopyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				imageCopyRegion.srcSubresource.layerCount = 1;
				imageCopyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				imageCopyRegion.dstSubresource.layerCount = 1;
				imageCopyRegion.extent.width = sourceImageVK->GetSpecification().Width;
				imageCopyRegion.extent.height = sourceImageVK->GetSpecification().Height;
				imageCopyRegion.extent.depth = 1;

				vkCmdCopyImage(cmd, sourceImageVK->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_SwapchainFrame->CurrentImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopyRegion);
			});
		//Transition swapchain image back into present
		m_CommandControl->ImmidiateSubmit(VulkanCommandControl::SubmitType::SUBMIT_TYPE_GRAPHICS_GRAPHICS, [=](VkCommandBuffer cmd) {
			VkImageMemoryBarrier barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.image = m_SwapchainFrame->CurrentImage;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.levelCount = 1;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = 1;

			VkPipelineStageFlags srcStage;
			VkPipelineStageFlags dstStage;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; //memory_write
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

			vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
			});
	}

	// State
	void VulkanRenderer::OnResize(uint32_t width, uint32_t height)
	{
		if (m_FramebufferResized)
			return;


		m_FramebufferResized = true;

		
		m_Specification.State.WindowWidth = m_FramebufferWidth = width;
		m_Specification.State.WindowHeight = m_FramebufferHeight = height;
	}

	void VulkanRenderer::OnViewportResize(uint32_t width, uint32_t height)
	{
		if (m_ViewportResized)
			return;

		m_ViewportResized = true;


		m_Specification.State.ViewportWidth = m_ViewportWidth = width;
		m_Specification.State.ViewportHeight = m_ViewportHeight = height;

		InitFinalImage(m_ViewportWidth, m_ViewportHeight);

		m_ViewportResized = false;
	}

	void VulkanRenderer::OnSwapchainRecreate()
	{
		VkExtent2D extent{ m_Swapchain->GetProperties().Width, m_Swapchain->GetProperties().Height };
		m_ImGui->OnSwapchainRecreate(Application::Get()->GetWindow().GetSwapchain()->GetImageViews(), extent);
	}

	// Commands
	void VulkanRenderer::BeginCommandBuffer(const void* commBuf)
	{
		PX_PROFILE_FUNCTION();


		m_ActiveCommandBuffer = (VkCommandBuffer)commBuf;
		m_SwapchainFrame->Commands.push_back(m_ActiveCommandBuffer);
		
		VkCommandBufferBeginInfo cmdBegin{};
		cmdBegin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmdBegin.pNext = nullptr;
		cmdBegin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		PX_CORE_VK_ASSERT(vkBeginCommandBuffer(m_ActiveCommandBuffer, &cmdBegin), VK_SUCCESS, "Failed to begin command buffer!");
		
#ifdef PX_DEBUG
		VkDebugUtilsObjectNameInfoEXT nameInfo{};
		nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
		nameInfo.objectType = VK_OBJECT_TYPE_COMMAND_BUFFER;
		nameInfo.objectHandle = (uint64_t)m_ActiveCommandBuffer;
		std::string debugName = "ActiveCommandBuffer_Frame" + std::to_string(GetCurrentFrameIndex());
		nameInfo.pObjectName = debugName.c_str();
		NameVkObject(VulkanContext::GetDevice()->GetVulkanDevice(), nameInfo);
#endif // DEBUG
	}
	void VulkanRenderer::EndCommandBuffer()
	{

		PX_CORE_VK_ASSERT(vkEndCommandBuffer(m_ActiveCommandBuffer), VK_SUCCESS, "Failed to record graphics command buffer!");
		m_ActiveCommandBuffer = VK_NULL_HANDLE;
	}
	
	void VulkanRenderer::InitCommandControl()
	{
		PX_PROFILE_FUNCTION();


		PX_CORE_INFO("VulkanRenderer::InitCommandControl: Starting...");

		m_CommandControl = CreateScope<VulkanCommandControl>();
		PX_CORE_ASSERT(m_CommandControl, "Failed to create CommandControl!");

		m_UploadContext = m_CommandControl->CreateUploadContext();
		PX_CORE_ASSERT(m_UploadContext, "Failed to get UploadContext!");		

		PX_CORE_INFO("VulkanRenderer::InitCommandControl: Completed.");
	}

	//Renderpass
	void VulkanRenderer::BeginRenderPass(Ref<RenderPass> renderPass)
	{
		PX_PROFILE_FUNCTION();
		
		m_ActiveRenderPass = std::dynamic_pointer_cast<VulkanRenderPass>(renderPass);
		Ref<VulkanFramebuffer> fb = std::dynamic_pointer_cast<VulkanFramebuffer>(m_ActiveRenderPass->GetSpecification().TargetFramebuffer);


		//TODO: Get from the fb
		std::array<VkClearValue, 2> clearColor = {};
		clearColor[0].color = { 0.3f, 0.3f, 0.3f, 0.0f };
		clearColor[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo info{};
		info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		info.pNext = nullptr;
		info.renderPass = m_ActiveRenderPass->GetRenderPass();
		info.framebuffer = fb->GetFramebuffer();
		info.renderArea.offset = { 0, 0 };
		info.renderArea.extent = { fb->GetSpecification().Width, fb->GetSpecification().Height };

		info.clearValueCount = static_cast<uint32_t>(clearColor.size());
		info.pClearValues = clearColor.data();

		vkCmdBeginRenderPass(m_ActiveCommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);

		// Bind pipeline. TODO: Behaviour if multiple pipelines are used during one renderpass (logic not supported yet)
		BindPipeline(renderPass->GetSpecification().Pipeline);
		vkCmdSetLineWidth(m_ActiveCommandBuffer, 1.0f);

		m_ActiveRenderPass->UpdateResourceOwnership(m_CurrentFrameIndex);

		auto& descriptors = m_ActiveRenderPass->GetDescriptorSets();
		auto& dynamicOffsets = m_ActiveRenderPass->GetDynamicOffsets(m_CurrentFrameIndex);

		std::vector<VkDescriptorSet> descriptorSets;
		for (auto& [number, set] : descriptors)
		{
			// Now bind global descriptor sets (all sets 0-2)
			if (number < 3)
			{
				descriptorSets.push_back(set.Sets[m_CurrentFrameIndex % (set.Sets.size())]);
			}
		}

		// TODO: catch dynamic descriptor sets and there offset
		vkCmdBindDescriptorSets(
			m_ActiveCommandBuffer, 
			VK_PIPELINE_BIND_POINT_GRAPHICS, 
			m_ActivePipeline->GetLayout(), 
			0,
			static_cast<uint32_t>(descriptorSets.size()), 
			descriptorSets.data(), 
			static_cast<uint32_t>(dynamicOffsets.size()),
			dynamicOffsets.data());

	}
	void VulkanRenderer::EndRenderPass()
	{
		vkCmdEndRenderPass(m_ActiveCommandBuffer);
		m_ActiveRenderPass = nullptr;
	}

	// Pipeline
	void VulkanRenderer::BindPipeline(Ref<Pipeline> pipeline)
	{
		PX_PROFILE_FUNCTION();


		m_ActivePipeline = std::dynamic_pointer_cast<VulkanPipeline>(pipeline);
		
		if (pipeline->GetSpecification().DynamicViewAndScissors)
		{
			uint32_t width = m_ViewportWidth;
			uint32_t height = m_ViewportHeight;
			VkViewport viewport{};
			viewport.x = 0.0f;
			viewport.y = static_cast<float>(height);
			viewport.width = static_cast<float>(width);
			viewport.height = -static_cast<float>(height);
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			vkCmdSetViewport(m_ActiveCommandBuffer, 0, 1, &viewport);

			VkRect2D scissor{};
			scissor.offset = { 0, 0 };
			scissor.extent = { width, height };
			vkCmdSetScissor(m_ActiveCommandBuffer, 0, 1, &scissor);
		}

		if (!m_QueryActive)
		{
			//m_QueryManager->BeginPipelineQuery("PipelineQueryPool", m_ActiveCommandBuffer, m_CurrentFrameIndex);
			m_QueryActive = true;
		}
		
		vkCmdBindPipeline(m_ActiveCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ActivePipeline->GetVulkanObj());
	}

	// Compute
	void VulkanRenderer::DispatchCompute(Ref<ComputePass> computePass, uint64_t totalElements, uint32_t workGroupWeightX, uint32_t workGroupWeightY, uint32_t workGroupWeightZ)
	{
		PX_PROFILE_FUNCTION();

		Ref<VulkanComputePass> vkComputePass = std::static_pointer_cast<VulkanComputePass>(computePass);
		m_ActiveComputePass = vkComputePass;

		VkCommandBuffer computeCmd = GetCurrentFrame().Commands.ComputeBuffer;

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.pNext = nullptr;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		beginInfo.pInheritanceInfo = nullptr;
		PX_CORE_VK_ASSERT(vkBeginCommandBuffer(computeCmd, &beginInfo), VK_SUCCESS, "Failed to end ComputeCommandbuffer!");

#ifdef PX_DEBUG
		VkDebugUtilsObjectNameInfoEXT nameInfo{};
		nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
		nameInfo.objectType = VK_OBJECT_TYPE_COMMAND_BUFFER;
		nameInfo.objectHandle = (uint64_t)computeCmd;
		std::string debugName = "CommandBuffer_" + computePass->GetSpecification().DebugName + "_Frame" + std::to_string(GetCurrentFrameIndex());
		nameInfo.pObjectName = debugName.c_str();
		NameVkObject(VulkanContext::GetDevice()->GetVulkanDevice(), nameInfo);
#endif // DEBUG
		
		auto& passSpecs = vkComputePass->GetSpecification();
		if (passSpecs.DoPerformanceQuery)
			m_QueryManager->RecordTimestamp(passSpecs.DebugName, m_CurrentFrameIndex, computeCmd);


		Ref<VulkanComputePipeline> vkComputePipeline = std::dynamic_pointer_cast<VulkanComputePipeline>(passSpecs.Pipeline);

		uint32_t computeFamIndex = VulkanContext::GetDevice()->GetQueueFamilies().ComputeFamilyIndex;
		uint32_t graphicsFamIndex = VulkanContext::GetDevice()->GetQueueFamilies().GraphicsFamilyIndex;
		
		vkComputePass->UpdateResourceOwnership(m_CurrentFrameIndex);
		
		if (passSpecs.DoPerformanceQuery)
			m_QueryManager->BeginPipelineQuery(passSpecs.DebugName, computeCmd, m_CurrentFrameIndex);

		vkCmdBindPipeline(computeCmd, VK_PIPELINE_BIND_POINT_COMPUTE, vkComputePipeline->GetVulkanObj());

		auto& descriptorSetMap = m_ActiveComputePass->GetDescriptorSets();
		auto& dynamicOffsets = m_ActiveComputePass->GetDynamicOffsets(m_CurrentFrameIndex);

		std::vector<VkDescriptorSet> descriptorSets;
		for (auto& [number, set] : descriptorSetMap)
		{
			descriptorSets.push_back(set.Sets[m_CurrentFrameIndex % (set.Sets.size())]);
		}

		vkCmdBindDescriptorSets(
			computeCmd, 
			VK_PIPELINE_BIND_POINT_COMPUTE, 
			vkComputePipeline->GetLayout(), 
			0, 
			static_cast<uint32_t>(descriptorSets.size()), 
			descriptorSets.data(), 
			static_cast<uint32_t>(dynamicOffsets.size()), 
			dynamicOffsets.data());

		auto& computeSpecs = vkComputePass->GetSpecification();
		float requiredWorkGroups = std::ceil((float)totalElements / (float)(computeSpecs.LocalWorkgroupSize.X * computeSpecs.LocalWorkgroupSize.Y * computeSpecs.LocalWorkgroupSize.Z));
		//PX_CORE_INFO("VulkanRenderer::DispatchCompute: Required Groups: {}", requiredWorkGroups);
		float workGroupSum = workGroupWeightX + workGroupWeightY + workGroupWeightZ;
		uint32_t x = std::ceil((float)workGroupWeightX * requiredWorkGroups / workGroupSum);
		uint32_t y = std::ceil((float)workGroupWeightY * requiredWorkGroups / workGroupSum);
		uint32_t z = std::ceil((float)workGroupWeightZ * requiredWorkGroups / workGroupSum);
		//PX_CORE_INFO("VulkanRenderer::DispatchCompute: GroupSize ({}, {}, {})", x, y, z);
		vkCmdDispatch(computeCmd, requiredWorkGroups, 1, 1);

		if (passSpecs.DoPerformanceQuery)
		{
			m_QueryManager->EndPipelineQuery(passSpecs.DebugName, computeCmd, m_CurrentFrameIndex);
			m_QueryManager->RecordTimestamp(passSpecs.DebugName, m_CurrentFrameIndex, computeCmd);
		}

		PX_CORE_VK_ASSERT(vkEndCommandBuffer(computeCmd), VK_SUCCESS, "Failed to end ComputeCommandbuffer!");

		VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submitInfo.pNext = nullptr;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &computeCmd;
		submitInfo.signalSemaphoreCount = 0;
		submitInfo.pSignalSemaphores = nullptr;
		submitInfo.waitSemaphoreCount = 0;
		submitInfo.pWaitSemaphores = nullptr;
		submitInfo.pWaitDstStageMask = {};


		PX_CORE_VK_ASSERT(vkQueueSubmit(VulkanContext::GetDevice()->GetQueueFamilies().Queues.ComputeQueue, 1, &submitInfo, GetCurrentFrame().ComputeFence), VK_SUCCESS, "Failed to submit compute pass");	
		
	}

	// GUI
	void VulkanRenderer::BeginGUIRenderPass()
	{
		if (m_FramebufferResized)
		{
			VkExtent2D extent{ m_Swapchain->GetProperties().Width, m_Swapchain->GetProperties().Height };
			m_ImGui->OnSwapchainRecreate(Application::Get()->GetWindow().GetSwapchain()->GetImageViews(), extent);
			// TODO: Find right place for flag
			m_FramebufferResized = false;
		}
		m_ImGui->BeginRenderPass(m_ActiveCommandBuffer, m_CurrentSwapchainImageIndex, { m_Swapchain->GetProperties().Width, m_Swapchain->GetProperties().Height });
	}
	void VulkanRenderer::EndGUIRenderPass()
	{
		m_ImGui->EndRenderPass();
	}
	
	void VulkanRenderer::DrawGUI()
	{
		PX_PROFILE_FUNCTION();


		m_ImGui->RenderDrawData();
	}
	
	// ImGui
	void VulkanRenderer::InitImGui()
	{
		auto& swapchain = Application::Get()->GetWindow().GetSwapchain();
		m_ImGui->Init(swapchain->GetImageViews(), swapchain->GetProperties().Width, swapchain->GetProperties().Height);
		Renderer::AddTimestampQuery("GUIPass", 2);
	}
	void VulkanRenderer::BeginImGuiFrame()
	{
		m_ImGui->BeginFrame();
	}
	void VulkanRenderer::EndImGuiFrame()
	{
		m_ImGui->EndFrame();
	}
	void VulkanRenderer::CreateDescriptors()
	{
		PX_PROFILE_FUNCTION();


		PX_CORE_INFO("VulkanRenderer::CreateDescriptors: Starting...");
		PX_CORE_INFO("Creating (global) DescriptorLayouts... ");
		//TODO:: refactor global descriptor set (layout) creation to MaterialSystem?

	// Descriptors
		//Global DescriptorLayoutCreation
		VkDescriptorSetLayoutBinding camBinding{};
		VkDescriptorSetLayoutBinding sceneBinding{};
		{
			camBinding.binding = 0;
			camBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			camBinding.descriptorCount = 1;
			camBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
			camBinding.pImmutableSamplers = nullptr;


			sceneBinding.binding = 1;
			sceneBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
			sceneBinding.descriptorCount = 1;
			sceneBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
			sceneBinding.pImmutableSamplers = nullptr;

			std::vector<VkDescriptorSetLayoutBinding> globalBindings = { camBinding, sceneBinding };


			VkDescriptorSetLayoutCreateInfo globalInfo{};
			globalInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			globalInfo.pNext = nullptr;

			globalInfo.flags = 0;
			globalInfo.bindingCount = static_cast<uint32_t>(globalBindings.size());
			globalInfo.pBindings = globalBindings.data();

			m_GlobalDescriptorSetLayout = VulkanContext::GetDescriptorLayoutCache()->CreateDescriptorLayout(&globalInfo, "GlobalUBOs");
		}
		for (uint32_t i = 0; i < m_Specification.MaxFramesInFlight; i++)
		{
			VulkanDescriptorBuilder builder = VulkanDescriptorBuilder::Begin(VulkanContext::GetDescriptorLayoutCache(), VulkanContext::GetDescriptorAllocator());
			VkDescriptorBufferInfo camInfo{};
			camInfo.buffer = m_Frames[i].CamUniformBuffer.Buffer;
			camInfo.offset = 0;
			camInfo.range = sizeof(CameraUniform);
			builder.BindBuffer(camBinding, &camInfo);


			VkDescriptorBufferInfo sceneInfo{};
			sceneInfo.buffer = m_SceneParameterBuffer.Buffer;
			sceneInfo.offset = 0;
			sceneInfo.range = sizeof(SceneUniform);
			builder.BindBuffer(sceneBinding, &sceneInfo);

			builder.Build(m_Frames[i].GlobalDescriptorSet, m_GlobalDescriptorSetLayout, "GlobalDS");
		}

		PX_CORE_INFO("Completed (global) DescriptorLayouts creation. ");
		PX_CORE_INFO("Creating Object DescriptorLayouts... ");
		
		VkDescriptorSetLayoutBinding objectBinding{};
		{
			objectBinding.binding = 0;
			objectBinding.descriptorCount = 1;
			objectBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			objectBinding.pImmutableSamplers = nullptr;
			objectBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		
			VkDescriptorSetLayoutCreateInfo objectInfo{};
			objectInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			objectInfo.pNext = nullptr;

			objectInfo.flags = 0;
			objectInfo.bindingCount = 0;
			objectInfo.pBindings = &objectBinding;

			m_ObjectDescriptorSetLayout = VulkanContext::GetDescriptorLayoutCache()->CreateDescriptorLayout(&objectInfo, "GlobalSSBOs");
		}
		for (uint32_t i = 0; i < m_Specification.MaxFramesInFlight; i++)
		{
			VulkanDescriptorBuilder builder = VulkanDescriptorBuilder::Begin(VulkanContext::GetDescriptorLayoutCache(), VulkanContext::GetDescriptorAllocator());
			VkDescriptorBufferInfo objectInfo{};
			objectInfo.buffer = m_Frames[i].ObjectBuffer.Buffer;
			objectInfo.offset = 0;
			objectInfo.range = sizeof(ObjectUniform) * MAX_OBJECTS;
			builder.BindBuffer(objectBinding, &objectInfo);

			builder.Build(m_Frames[i].ObjectDescriptorSet, m_ObjectDescriptorSetLayout, "ObjectDS");
		}


		PX_CORE_INFO("Completed Object DescriptorLayouts creation. ");
		PX_CORE_INFO("Creating Particle DescriptorLayouts... ");

// 		VkDescriptorSetLayoutBinding particleBinding{};
// 		{
// 			particleBinding.binding = 0;
// 			particleBinding.descriptorCount = 1;
// 			particleBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
// 			particleBinding.pImmutableSamplers = nullptr;
// 			particleBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
// 
// 			VkDescriptorSetLayoutCreateInfo particleInfo{};
// 			particleInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
// 			particleInfo.pNext = nullptr;
// 
// 			particleInfo.flags = 0;
// 			particleInfo.bindingCount = 0;
// 			particleInfo.pBindings = &particleBinding;
// 
// 			m_ObjectDescriptorSetLayout = VulkanContext::GetDescriptorLayoutCache()->CreateDescriptorLayout(&particleInfo);
// 		}
// 		for (uint32_t i = 0; i < m_Specification.MaxFramesInFlight; i++)
// 		{
// 			VulkanDescriptorBuilder builder = VulkanDescriptorBuilder::Begin(VulkanContext::GetDescriptorLayoutCache(), VulkanContext::GetDescriptorAllocator());
// 			VkDescriptorBufferInfo particleInfo{};
// 			particleInfo.buffer = m_Frames[i].ParticleBuffer.Buffer;
// 			particleInfo.offset = 0;
// 			particleInfo.range = 0;
// 			//particleInfo.range = sizeof(ParticleLayout) * MAX_PARTICLES;
// 			builder.BindBuffer(particleBinding, &particleInfo);
// 
// 			builder.Build(m_Frames[i].ParticleDescriptorSet, m_ParticleDescriptorSetLayout, "ParticleDS");
// 		}


		PX_CORE_INFO("Completed Particle DescriptorLayouts creation. ");

		PX_CORE_INFO("Creating TextureArray DescriptorLayouts... ");

		//TextureDescriptorLayoutCreation
		VkDescriptorSetLayoutBinding samplerBinding{};
		VkDescriptorSetLayoutBinding textureArrayBinding{};
		{
			samplerBinding.binding = 0;
			samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
			samplerBinding.descriptorCount = 1;
			samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			samplerBinding.pImmutableSamplers = nullptr;
		

			textureArrayBinding.binding = 1;
			textureArrayBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			textureArrayBinding.descriptorCount = m_TextureSystem->GetConfig().MaxTextureSlots;
			textureArrayBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			textureArrayBinding.pImmutableSamplers = nullptr;

			std::vector<VkDescriptorSetLayoutBinding> textureBindings = { samplerBinding, textureArrayBinding };

			VkDescriptorSetLayoutCreateInfo textureInfo{};
			textureInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			textureInfo.pNext = nullptr;

			textureInfo.flags = 0;
			textureInfo.bindingCount = static_cast<uint32_t>(textureBindings.size());
			textureInfo.pBindings = textureBindings.data();

			m_TextureDescriptorSetLayout = VulkanContext::GetDescriptorLayoutCache()->CreateDescriptorLayout(&textureInfo, "Textures");
		}

		auto& activeTextures = m_TextureSystem->GetActiveTextures();
		//TODO: Get maxImageSlots from config as const
		std::vector<VkDescriptorImageInfo> imageInfos;	
		imageInfos.resize(m_TextureSystem->GetConfig().MaxTextureSlots);
		for (uint32_t i = 0; i < imageInfos.size(); i++)
		{
			Ref<Image2D> image = activeTextures[i]->GetImage();
			//TODO: Define BindImages for arrays of image infos!
			Ref<VulkanImage2D> vkimage = std::dynamic_pointer_cast<VulkanImage2D>(activeTextures[i]->GetImage());
			imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfos[i].imageView = vkimage->GetImageView();
			imageInfos[i].sampler = nullptr;
		}

		for (uint32_t i = 0; i < m_Specification.MaxFramesInFlight; i++)
		{	
			VulkanDescriptorBuilder builder = VulkanDescriptorBuilder::Begin(VulkanContext::GetDescriptorLayoutCache(), VulkanContext::GetDescriptorAllocator());

			VkDescriptorImageInfo samplerInfo{};
			samplerInfo.sampler = m_TextureSampler;

			builder.BindImage(samplerBinding, &samplerInfo);
			builder.BindImage(textureArrayBinding, imageInfos.data());

			builder.Build(m_Frames[i].TextureDescriptorSet, m_TextureDescriptorSetLayout, "TextureArrayDS");
		}




		PX_CORE_INFO("Completed textureArray DescriptorLayouts creation. ");
		PX_CORE_INFO("VulkanRenderer::CreateDescriptors: Completed.");
	}

	void* VulkanRenderer::GetGUIDescriptorSet(Ref<Image2D> image) const
	{
		Ref<VulkanImage2D> vkImage = std::dynamic_pointer_cast<VulkanImage2D>(image);

		if ((VkDescriptorSet)vkImage->GetDescriptorSet())
			m_ImGui->FreeImGuiDescriptorSet((VkDescriptorSet)vkImage->GetDescriptorSet());

		/*vkImage->TransitionImageLayout(
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT,
			VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT
		);*/
		
		vkImage->SetDescriptorSet(m_ImGui->GetImGUIDescriptorSet(vkImage->GetImageView(), vkImage->GetSampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
		return vkImage->GetDescriptorSet();
	}

	
	//TEMP
	// Samplers
	void VulkanRenderer::CreateSamplers()
	{
		PX_PROFILE_FUNCTION();


		// Texture Sampler
		{
			VkSamplerCreateInfo samplerInfo{};
			samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			samplerInfo.magFilter = VK_FILTER_LINEAR;
			samplerInfo.minFilter = VK_FILTER_LINEAR;
			samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerInfo.anisotropyEnable = VK_TRUE;

			VkPhysicalDeviceProperties properties;
			vkGetPhysicalDeviceProperties(VulkanContext::GetDevice()->GetPhysicalDevice(), &properties);
			samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
			samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
			samplerInfo.unnormalizedCoordinates = VK_FALSE;
			samplerInfo.compareEnable = VK_TRUE;
			samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
			samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			samplerInfo.mipLodBias = 0.0f;
			samplerInfo.maxLod = 0.0f;
			samplerInfo.minLod = 0.0f;
			PX_CORE_VK_ASSERT(vkCreateSampler(VulkanContext::GetDevice()->GetVulkanDevice(), &samplerInfo, nullptr, &m_TextureSampler), VK_SUCCESS, "Failed to create texture sampler!");

#ifdef PX_DEBUG
			VkDebugUtilsObjectNameInfoEXT nameInfo{};
			nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
			nameInfo.objectType = VK_OBJECT_TYPE_SAMPLER;
			nameInfo.objectHandle = (uint64_t)m_TextureSampler;
			nameInfo.pObjectName = "TextureSampler";
			NameVkObject(VulkanContext::GetDevice()->GetVulkanDevice(), nameInfo);
#endif // DEBUG
		}	
	}
	// Descriptors
	size_t VulkanRenderer::PadUniformBuffer(size_t inputSize, size_t minGPUBufferOffsetAlignment)
	{
		size_t alignedSize = inputSize;
		if (minGPUBufferOffsetAlignment > 0)
		{
			alignedSize = (alignedSize + minGPUBufferOffsetAlignment - 1) & ~(minGPUBufferOffsetAlignment - 1);
		}
		return alignedSize;
	}
			

	void VulkanRenderer::ComputeSubmit()
	{
		PX_PROFILE_FUNCTION();


		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &GetCurrentFrame().Commands.ComputeBuffer;

		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &GetCurrentFrame().Semaphores.ComputeFinishedSemaphore;

		PX_CORE_VK_ASSERT(vkQueueSubmit(VulkanContext::GetDevice()->GetQueueFamilies().Queues.ComputeQueue, 1, &submitInfo, GetCurrentFrame().ComputeFence), VK_SUCCESS, "Failed to submit compute commands!");
	}

	// Debugging and Performance
	void VulkanRenderer::StartTimestampQuery(const std::string& name)
	{
		if (m_ActiveCommandBuffer == VK_NULL_HANDLE)
		{
			PX_CORE_WARN("VulkanRenderer::StartTimestampQuery: No command buffer active!");
			return;
		}
		m_QueryManager->RecordTimestamp(name, m_CurrentFrameIndex, m_ActiveCommandBuffer);
	}
	void VulkanRenderer::StopTimestampQuery(const std::string& name)
	{
		if (m_ActiveCommandBuffer == VK_NULL_HANDLE)
		{
			PX_CORE_WARN("VulkanRenderer::StartTimestampQuery: No command buffer active!");
			return;
		}
		m_QueryManager->RecordTimestamp(name, m_CurrentFrameIndex, m_ActiveCommandBuffer);
	}

	void VulkanRenderer::AddTimestampQuery(const std::string& name, uint32_t count)
	{
		m_QueryManager->AddTimestampQuery(name, count);
	}

	void VulkanRenderer::AddPipelineStatisticsQuery(const std::string& name, const std::string& computeStatQueryPoolName)
	{
		m_QueryManager->AddPipelineStatisticsQuery(name, computeStatQueryPoolName);
	}

// Resource Creation and Getters
	void VulkanRenderer::InitPerformanceQueryPools()
	{
		m_QueryManager = CreateRef<VulkanQueryManager>();

		m_QueryManager->CreateTimestampQueryPools(m_Specification.MaxFramesInFlight, 20);
		m_QueryManager->CreatePipelineStatisticsQueryPool("PipelineQueryPool", m_Specification.MaxFramesInFlight, VK_PIPELINE_BIND_POINT_GRAPHICS);
		m_QueryManager->CreatePipelineStatisticsQueryPool( m_ComputeStatisticsQueryPoolName, m_Specification.MaxFramesInFlight, VK_PIPELINE_BIND_POINT_COMPUTE);
	}

	void VulkanRenderer::GetQueryResults(uint32_t frameIdx)
	{
		m_Statistics.PipelineStats["PipelineQueryPool"] = m_QueryManager->GetPipelineQueryResults("PipelineQueryPool", frameIdx);
		m_Statistics.PipelineStats[m_ComputeStatisticsQueryPoolName] = m_QueryManager->GetPipelineQueryResults(m_ComputeStatisticsQueryPoolName, frameIdx);
		m_Statistics.TimestampResults = m_QueryManager->GetTimestampQueryResults(frameIdx);
	}

	void VulkanRenderer::InitFinalImage(uint32_t width, uint32_t height)
	{
		PX_PROFILE_FUNCTION();

		if (width <= 0 || height <= 0)
		{
			PX_CORE_WARN("VulkanRenderer::InitFinalImage: Cannot create image with extent {}, {}", width, height);
			return;
		}

		if (m_FinalImages[m_CurrentFrameIndex])
		{
			m_FinalImages[m_CurrentFrameIndex]->Free();
			m_FinalImages[m_CurrentFrameIndex] = nullptr;
		}

		ImageSpecification imageSpec{};
		imageSpec.DebugName = "Renderer::FinalImage";
		imageSpec.Width = width;
		imageSpec.Height = height;
		imageSpec.ChannelCount = 4;
		imageSpec.Format = ImageFormat::RGBA8;
		imageSpec.Memory = MemoryUtils::MemoryUsage::GPU_ONLY;
		imageSpec.MipLevels = 1;
		imageSpec.Tiling = ImageTiling::OPTIMAL;
		imageSpec.Usages = { ImageUsage::SAMPLED, ImageUsage::COPY_DST };
		imageSpec.DedicatedSampler = true;
		imageSpec.CreateDescriptorOnInit = false;
		m_FinalImages[m_CurrentFrameIndex] = CreateRef<VulkanImage2D>(imageSpec);
	}

	Ref<Image2D> VulkanRenderer::GetFinalImage(uint32_t frameIndex) const
	{
		if (!(frameIndex < m_FinalImages.size()))
		{
			PX_CORE_WARN("VulkanRenderer::GetFinalImage: Index {} out of scope. Must be <= MaxFrames! Returning nullptr!", frameIndex);
			return nullptr;
		}
		if (m_FinalImages[frameIndex])
			return m_FinalImages[frameIndex];
		else
		{
			PX_CORE_WARN("VulkanRenderer::GetFinalImage: Final image at frame {} not yet created! Returning nullptr!", frameIndex);
			return nullptr;
		}
	}

	FrameData& VulkanRenderer::GetFrame(uint32_t index)
	{
		PX_CORE_ASSERT(index < m_Frames.size(), "Index too big!");
		return m_Frames[index];
	}

}
