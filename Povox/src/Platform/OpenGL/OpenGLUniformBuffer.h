#pragma once
#include "Povox/Renderer/UniformBuffer.h"

namespace Povox {

	class OpenGLUniformbuffer : public Uniformbuffer
	{
	public:
		OpenGLUniformbuffer(uint32_t size, uint32_t binding);
		virtual ~OpenGLUniformbuffer();

		virtual void SetData(const void* data, uint32_t size, uint32_t offset = 0) override;

	private:
		uint32_t m_RendererID = 0;
	};
}
