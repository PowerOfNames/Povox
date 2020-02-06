#include "pxpch.h"
#include "Povox/Renderer/Texture.h"

#include "Povox/Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLTexture.h"

namespace Povox {

	Povox::Ref<Povox::Texture2D> Texture2D::Create(uint32_t width, uint32_t height)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::NONE:
		{
			PX_CORE_ASSERT(false, "RendererAPI::NONE is not supported!");
			return nullptr;
		}
		case RendererAPI::API::OpenGL:
		{
			return CreateRef<OpenGLTexture2D>(width, height);
		}
		}
		PX_CORE_ASSERT(false, "Unknown RendererAPI");
		return nullptr;
	}

	Ref<Texture2D> Texture2D::Create(const std::string& path)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::NONE:
		{
			PX_CORE_ASSERT(false, "RendererAPI::NONE is not supported!");
			return nullptr;
		}
		case RendererAPI::API::OpenGL:
		{
			return CreateRef<OpenGLTexture2D>(path);
		}
		}
		PX_CORE_ASSERT(false, "Unknown RendererAPI");
		return nullptr;
	}
}