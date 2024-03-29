#pragma once
#include "Povox/Renderer/Framebuffer.h"

#include "Platform/Vulkan/VulkanUtilities.h"
#include "Platform/Vulkan/VulkanImage2D.h"


namespace Povox {

	struct FramebufferAttachment
	{
		Ref<VulkanImage2D> Image = nullptr;

		VkAttachmentDescription Description{};

		bool HasDepth()
		{
			std::vector<VkFormat> formats =
			{
				VK_FORMAT_D32_SFLOAT,
				VK_FORMAT_D32_SFLOAT_S8_UINT,
				VK_FORMAT_D24_UNORM_S8_UINT
			};
			return std::find(formats.begin(), formats.end(), VulkanUtils::GetVulkanImageFormat(Image->GetSpecification().Format)) != std::end(formats);
		}

		bool HasStencil()
		{
			std::vector<VkFormat> formats =
			{
				VK_FORMAT_S8_UINT,
				VK_FORMAT_D32_SFLOAT_S8_UINT
			};
			return std::find(formats.begin(), formats.end(), VulkanUtils::GetVulkanImageFormat(Image->GetSpecification().Format)) != std::end(formats);
		}

		bool HasDepthStencil()
		{
			return (HasDepth() || HasStencil());
		}
	};

	class VulkanFramebuffer : public Framebuffer
	{
	public:
		VulkanFramebuffer();
		VulkanFramebuffer(const FramebufferSpecification& specs);
		virtual ~VulkanFramebuffer();
				
		virtual void Recreate(uint32_t width = 0, uint32_t height = 0) override;
		void Destroy();
		
		virtual inline const FramebufferSpecification& GetSpecification() const override { return m_Specification; };

		virtual inline std::vector<Ref<Image2D>>& GetColorAttachments() override { return m_ColorAttachments; }
		virtual const Ref<Image2D> GetColorAttachment(size_t index = 0) override;
		virtual inline const Ref<Image2D> GetDepthAttachment() override { return m_DepthAttachment; };
		
		inline uint32_t GetWidth() { return m_Specification.Width; }
		inline uint32_t GetHeight() { return m_Specification.Height; }

		inline bool Resizable() const { return m_Specification.Resizable; }

		inline VkFramebuffer GetFramebuffer() { return m_Framebuffer; }
		inline VkRenderPass GetRenderPass() { return m_RenderPass; }

		virtual const std::string& GetDebugName() const override { return m_Specification.DebugName; }
	private:
		void CreateAttachments();
		void CreateRenderPass();
		void CreateFramebuffer();
	private:
		FramebufferSpecification m_Specification;
		VkFramebuffer m_Framebuffer = VK_NULL_HANDLE;
		VkRenderPass m_RenderPass = VK_NULL_HANDLE;

		std::vector<Ref<Image2D>> m_ColorAttachments;
		Ref<Image2D> m_DepthAttachment = nullptr;
	};
}
