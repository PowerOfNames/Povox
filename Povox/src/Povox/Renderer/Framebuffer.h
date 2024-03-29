#pragma once
#include "Image2D.h"

namespace Povox {

	struct FramebufferAttachmentSpecification
	{
		ImageFormat Format = ImageFormat::None;
		MemoryUtils::MemoryUsage Memory = MemoryUtils::MemoryUsage::GPU_ONLY;
		ImageTiling Tiling = ImageTiling::LINEAR;
		std::vector<ImageUsage> Usages = { ImageUsage::COLOR_ATTACHMENT };


		// TODO: Filtering/wrapping to choose or create right sampler
	};

	struct FramebufferAttachments
	{
		FramebufferAttachments() = default;
		FramebufferAttachments(std::initializer_list<FramebufferAttachmentSpecification> attachments)
			: Attachments(attachments) {}
		
		std::vector<FramebufferAttachmentSpecification> Attachments;
	};

	class Framebuffer;
	struct FramebufferSpecification
	{
		uint32_t Width = 0, Height = 0;
		struct
		{
			float X = 1.0f;
			float Y = 1.0f;
		} Scale;
		FramebufferAttachments Attachments;
		uint32_t ColorAttachmentCount = 0;
		bool HasDepthAttachment = false;

		std::vector<Ref<Image2D>> OriginalImages;

		uint32_t Samples = 1;

		std::string Name = "None";

		bool Resizable = true;
		bool SwapChainTarget = false;
		Ref<Framebuffer> OriginalFramebuffer = nullptr;

		std::string DebugName = "Framebuffer";
	};

	class Framebuffer
	{
	public:
		virtual ~Framebuffer() = default;

		virtual void Recreate(uint32_t width, uint32_t height) = 0;


		virtual const FramebufferSpecification& GetSpecification() const = 0;

		virtual const std::vector<Ref<Image2D>>& GetColorAttachments() = 0;
		virtual const Ref<Image2D> GetColorAttachment(size_t index = 0) = 0;
		virtual const Ref<Image2D> GetDepthAttachment() = 0;
		
		virtual const std::string& GetDebugName() const = 0;

		static Ref<Framebuffer> Create();
		static Ref<Framebuffer> Create(const FramebufferSpecification& spec);
	};

}
