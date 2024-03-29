#pragma once

#include "Povox/Renderer/GraphicsContext.h"

struct GLFWwindow;

namespace Povox {

	class OpenGLContext : public GraphicsContext
	{
	public:
		OpenGLContext(GLFWwindow* windowHandle);

		virtual void Init() override;
		virtual void Shutdown() override;
	private:
		GLFWwindow* m_WindowHandle;
	};
}
