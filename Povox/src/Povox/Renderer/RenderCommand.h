#pragma once

#include "Povox/Renderer/RendererAPI.h"

namespace Povox {

	class RenderCommand
	{
	public:
		inline static void Init() { s_RendererAPI->Init(); }

		inline static void SetClearColor(glm::vec4 clearColor) { s_RendererAPI->SetClearColor(clearColor); }
		inline static void Clear() { s_RendererAPI->Clear(); }

		inline static void SetCulling(bool active, bool clockwise) { s_RendererAPI->SetCulling(active, clockwise); }
		inline static void SetDrawMode(bool active) { s_RendererAPI->SetDrawMode(active); }

		inline static void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
		{
			s_RendererAPI->SetViewport(x, y, width, height);
		}

		inline static void DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount) { s_RendererAPI->DrawIndexed(vertexArray, indexCount); }

	private:
		static RendererAPI* s_RendererAPI;
	};
}
